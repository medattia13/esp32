//TODO//
// add arabic character handling using ucs2ToUtf8
//// TODO: Move phonebook functionality into a separate PhoneBook class.
//       Keep SIM800 focused on modem communication.

#include "SIM800.h"
#include <ctype.h>
#include <string.h>
#define SIM800_RX 16
#define SIM800_TX 17

constexpr uint32_t MODEM_BAUD = 9600;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t MODEM_BOOT_TIME = 5000;
constexpr size_t PHONE_BUF_SIZE = 32;

SIM800::SIM800()
    : modem(2)
    , state(BOOTING)
    , smsState(SMS_IDLE)
    , linePos(0)
    , bootStep(0)
    , bootStart(0)
    , debugMode(false)
{
    lineBuffer[0] = '\0';
}

void SIM800::begin()
{
    modem.begin(MODEM_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);

    Serial.println();
    Serial.println("SIM800 starting...");

    bootStart = millis();
}
void SIM800::update()
{
    processSerial();

    checkATTimeout();
    if (debugMode) {
        handleDebugInput();
        return;
    }
    switch (state) {
    case BOOTING: {
        static bool commandSent = false;

        if (!commandSent && !atCommand.active && millis() - bootStart >= MODEM_BOOT_TIME) {
            if (bootStep == 0) {
                if (sendAT("AT")) {
                    commandSent = true;
                }
            } else {
                if (sendAT("ATE0")) {
                    commandSent = true;
                }
            }

            commandSent = true;
        }

        if (commandSent && atFinished()) {
            commandSent = false;

            if (atResult() != AT_OK) {
                Serial.println("SIM800 failed");
                state = MODEM_ERROR;
                break;
            }

            if (bootStep == 0) {
                bootStep = 1;
            } else {
                Serial.println("SIM800 ready");
                state = READY;
            }

            atCommand.finished = false;
        }

        break;
    }
    case READY: {
        static bool initialized = false;
        static uint8_t initStep = 0;

        // modem initialization sequence
        if (!initialized) {
            if (!atCommand.active && !atCommand.finished) {
                switch (initStep) {
                case 0:
                    sendAT("AT+CPIN?", 3000);
                    break;

                case 1:
                    sendAT("AT+CREG?", 3000);
                    break;

                case 2:
                    sendAT("AT+CSQ", 2000);
                    break;
                case 3:
                    sendAT("AT+CLIP=1", 2000);
                    break;
                case 4:
                    initialized = true;
                    state = WAIT_MODE;
                    printMenu();
                    break;
                }
            }

            if (atFinished()) {
                if (atResult() != AT_OK) {
                    state = MODEM_ERROR;
                    break;
                }

                atCommand.finished = false;
                initStep++;
            }

            break;
        }

        // User input
        if (Serial.available()) {
            char input[PHONE_BUF_SIZE];

            size_t len = Serial.readBytesUntil('\n', input, PHONE_BUF_SIZE - 1);

            input[len] = '\0';

            while (len > 0 && (input[len - 1] == '\r' || input[len - 1] == '\n')) {
                input[--len] = '\0';
            }

            if (strcasecmp(input, "DEBUG") == 0) {
                handleDebugInput();
            }

            else if (strcasecmp(input, "Hang") == 0) {
                hangup();
            }

            else if (strcasecmp(input, "READSMS") == 0)
            {
                readSMS();
            }

            else if (strcasecmp(input, "SMS") == 0)
            {
                Serial.println("Enter phone number:");
                state = SMS_NUMBER;
            }

            else if (strcasecmp(input, "USSD") == 0)
            {
                Serial.println("Enter USSD code:");
                state = USSD_INPUT;
            }
            else if (strcasecmp(input, "CONTACTS") == 0)
            {
                
                    state = PHONEBOOK_MENU;
    phonebookMenu = PB_MENU_MAIN;
    printPhonebookMenu();
            }

            else {
                Serial.println("Invalid input");
            }
        }

        break;
    }

    case DIALING: {
        if (atFinished()) {
            if (atResult() == AT_TIMEOUT) {
                Serial.println("Dial command timeout");
                state = READY;
            }

            atCommand.finished = false;
        }

        if (Serial.available()) {
            char c = Serial.read();

            if (c == 'h' || c == 'H') {
                hangup();
            }
        }

        break;
    }

    case IN_CALL: {
        if (Serial.available()) {
            char c = Serial.read();
            if(c=='h'||c=='H')
            {
                hangup();
            }

        }

        break;
    }

    case MODEM_ERROR: {
        Serial.println("Modem error");

        while (true) {
            // stop
        }

        break;
    }
    case WAIT_MODE: {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            input.toUpperCase();
            if (input == "CALL")
            {
                Serial.println("Enter phone number");
                Serial.println("Example: +21612345678");
                state = CALL_NUMBER;
            } else if (input == "SMS") {
                Serial.println("Phone number:");
                state = SMS_NUMBER;
            } else if (input == "USSD") {
                Serial.println("Enter USSD code:");
                state = USSD_INPUT;
            } else if (input == "READSMS") {
                readSMS();
                printMenu();
            }
            else if(input=="DEBUG")
            {
                debugMode=true;
            }
            else {
                printMenu();
            }
        }
        break;
    }
    case CALL_NUMBER:
    {
        if (Serial.available())
        {
            phoneNumber = Serial.readStringUntil('\n');
            phoneNumber.trim();

            if (dial(phoneNumber.c_str()))
            {
                phoneNumber = "";
                state = DIALING;
            }
            else
            {
                Serial.println("Invalid phone number.");
                state = WAIT_MODE;
            }
        }
        break;
    }

    case SMS_NUMBER: {
        if (Serial.available()) {
            phoneNumber = Serial.readStringUntil('\n');
            phoneNumber.trim();

            Serial.println("Message:");
            state = SMS_MESSAGE;
        }
        break;
    }

    case SMS_MESSAGE: {
        if (Serial.available()) {
            message = Serial.readStringUntil('\n');
            message.trim();

            if (sendSMS(phoneNumber.c_str(), message.c_str())) {
                phoneNumber = "";
                message = "";
            } else {
                Serial.println("Unable to start SMS.");

                phoneNumber = "";
                message = "";

                state = WAIT_MODE;
                printMenu();
            }
        }
        break;
    }
    case USSD_INPUT:
    {
        if (Serial.available())
        {
            String code = Serial.readStringUntil('\n');
            code.trim();

            if(sendUSSD(code))
            {
                Serial.println("Waiting for USSD reply...");
            }
            else
            {
                Serial.println("USSD failed.");
                state = WAIT_MODE;
                printMenu();
            }
        }

        break;
    }

    case USSD_PENDING:
    {
        break;
    }
case PHONEBOOK_MENU:
{
    handlePhonebookInput();
    break;
}

}
}
bool SIM800::dial(const char *number)
{


    if (!validNumber(number))
        return false;

    char cmd[48];

    snprintf(cmd, sizeof(cmd), "ATD%s;", number);

    Serial.print("Calling ");
    Serial.println(number);

    if (sendAT(cmd, 30000)) {
        state = DIALING;
        return true;
    }

    return false;
}
bool SIM800::hangup()
{
    return sendAT("ATH",5000);
}

bool SIM800::sendSMS(const char *number, const char *message)
{
    if (smsState != SMS_IDLE) {
        Serial.print("SMS busy, state = ");
        Serial.println((int)smsState);
        return false;
    }

    if (!validNumber(number)) {
        Serial.println("Invalid phone number");
        return false;
    }

    strncpy(smsNumber, number, sizeof(smsNumber) - 1);
    smsNumber[sizeof(smsNumber) - 1] = '\0';

    strncpy(smsText, message, sizeof(smsText) - 1);
    smsText[sizeof(smsText) - 1] = '\0';

    Serial.println("Starting SMS...");

    if (!sendAT("AT+CMGF=1", 3000)) {
        Serial.println("Could not start AT+CMGF=1");
        smsState = SMS_IDLE;   // IMPORTANT
        return false;
    }

    smsState = SMS_WAIT_TEXTMODE;

    return true;
}

bool SIM800::sendUSSD(String code)
{
    char cmd[40];

    snprintf(cmd,
             sizeof(cmd),
             "AT+CUSD=1,\"%s\"",
             code.c_str());

    // Cancel any active USSD session
    //sim800.println("AT+CUSD=2");

    if(sendAT(cmd,10000))
    {
        state = USSD_PENDING;
        return true;
    }
    return false;
}
void SIM800::printMenu() {
    Serial.println();
    Serial.println("========== MENU ==========");
    Serial.println("CALL        - Voice call");
    Serial.println("SMS         - Send SMS");
    Serial.println("USSD        - Send USSD");
    Serial.println("READSMS     - Read SMS");
    Serial.println("DEBUG       - AT debug");
    Serial.println("CONTACTS    - Phonebook");
    Serial.println("==========================");
}
void SIM800::processLine(const char *line)
{ // TODO: Replace strcpy() in processLine() response handling with memcpy()
    //       to avoid copying the null terminator twice and make length handling cleaner.

    Serial.print("<< ");
    Serial.println(line);
    if (strncmp(line, "+CLIP:", 6) == 0)
    {
        const char *start = strchr(line, '"');

        if (start)
        {
            start++;

            const char *end = strchr(start, '"');

            if (end)
            {
                char number[32];

                size_t len = end - start;

                if (len >= sizeof(number))
                    len = sizeof(number) - 1;

                memcpy(callerNumber, start, len);
                callerNumber[len] = '\0';

                Serial.print("Incoming call from: ");
                Serial.println(callerNumber);

            }
        }

        return;
    }
    // Unsolicited events
    if (strcmp(line, "CONNECT") == 0) {
        state = IN_CALL;
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_OK;
        return;
    } else if (strcmp(line, "NO CARRIER") == 0) {
        state = WAIT_MODE;
        printMenu();
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;
        return;
    } else if (strcmp(line, "BUSY") == 0) {
        state = WAIT_MODE;
        printMenu();

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;
        return;
    } else if (strcmp(line, "RING") == 0) {
        incomingCall = true;
        Serial.println("Incoming call");
        return;
    } else if (strcmp(line, "NO ANSWER") == 0) {
        Serial.println("No answer");

        state = WAIT_MODE;
        printMenu();

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;
        return;
    } else if (strcmp(line, "NO DIALTONE") == 0) {
        Serial.println("No dial tone");

        state = WAIT_MODE;
        printMenu();

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;
        return;
    }

    else if (strcmp(line, "MO RING") == 0) {
        Serial.println("Remote phone is ringing");
        return;
    }
    // AT command final responses
    if (strcmp(line, "OK") == 0)
    {
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_OK;
// ========================================================
// PHONEBOOK STATE MACHINE
// ========================================================

if (phonebookState == PB_WAIT_STORAGE)
{
    atCommand.finished = false;

    if (phonebookMenu == PB_MENU_VIEW ||
        phonebookMenu == PB_MENU_SEARCH)
    {
        phonebookState = PB_WAIT_LIST;

        sendAT("AT+CPBR=1,20", 5000);
    }
    else
    {
        phonebookState = PB_WAIT_READ;

        char cmd[32];

        snprintf(
            cmd,
            sizeof(cmd),
            "AT+CPBR=%u",
            phonebookIndex
        );

        sendAT(cmd, 5000);
    }

    return;
}



        switch (smsState)
        {
        case SMS_WAIT_TEXTMODE:
            smsState = SMS_WAIT_CHARSET;
            sendAT("AT+CSCS=\"GSM\"",3000);
            return;

        case SMS_WAIT_CHARSET:
        {
            char cmd[48];
            snprintf(cmd,sizeof(cmd),"AT+CMGS=\"%s\"",smsNumber);

            smsState = SMS_WAIT_PROMPT;
            sendAT(cmd,5000);
            return;
        }

        case SMS_WAIT_RESULT:
            smsNumber[0] = '\0';
            smsText[0] = '\0';
            smsState = SMS_IDLE;
            Serial.println("SMS sent successfully.");
            state = WAIT_MODE;
            printMenu();
            return;
        case SMS_READING:
                    smsState = SMS_WAIT_LIST;
            sendAT("AT+CMGL=\"ALL\"",5000);
            return;
        case SMS_WAIT_LIST:
            // This OK belongs to AT+CMGL="ALL".
            // Do NOT send CMGL again.
            smsState = SMS_IDLE;

            Serial.println("Finished reading SMS.");

            state = WAIT_MODE;
            printMenu();
            return;

        default:
            break;
        }

        return;
    }
if (phonebookState == PB_WAIT_WRITE)
{
    phonebookState = PB_IDLE;

    Serial.println();
    Serial.println("Contact saved successfully.");

    phonebookMenu = PB_MENU_MAIN;
    state = PHONEBOOK_MENU;

    printPhonebookMenu();

    return;
}

if (phonebookState == PB_WAIT_DELETE)
{
    phonebookState = PB_IDLE;

    Serial.println();
    Serial.println("Contact deleted successfully.");

    phonebookMenu = PB_MENU_MAIN;
    state = PHONEBOOK_MENU;

    printPhonebookMenu();

    return;
}

if (phonebookState == PB_WAIT_LIST)
{
    phonebookState = PB_IDLE;

    Serial.println();
    Serial.println("Finished reading phonebook.");

    if (phonebookEntryCount == 0)
    {
        Serial.println("No contacts found.");
    }

    phonebookMenu = PB_MENU_MAIN;

    state = PHONEBOOK_MENU;

    printPhonebookMenu();

    return;
}

if (phonebookState == PB_WAIT_READ)
{
    phonebookState = PB_IDLE;

    // A +CPBR response should already have populated
    // lastPhonebookEntry.

    if (lastPhonebookEntry.index != 0)
    {
        printPhonebookEntry(lastPhonebookEntry);

        // Call contact?
        if (phonebookMenu == PB_MENU_CALL)
        {
            Serial.print("Calling ");
            Serial.println(lastPhonebookEntry.number);

            phoneNumber = lastPhonebookEntry.number;

            phonebookMenu = PB_MENU_MAIN;

            dial(phoneNumber.c_str());

            return;
        }
    }
    else
    {
        Serial.println("Contact not found.");
    }

    phonebookMenu = PB_MENU_MAIN;

    state = PHONEBOOK_MENU;

    printPhonebookMenu();

    return;
}

    if(strncmp(line, "+CUSD:", 6) == 0)
    {
        Serial.println("USSD reply:");
        Serial.println(line);
        state = WAIT_MODE;
        printMenu();
        return;
    }


    if (strcmp(line, "ERROR") == 0)
{
    smsState = SMS_IDLE;

    if (phonebookState != PB_IDLE)
    {
        Serial.println("Phonebook operation failed.");

        phonebookState = PB_IDLE;
        phonebookMenu = PB_MENU_MAIN;

        state = PHONEBOOK_MENU;

        printPhonebookMenu();

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;

        return;
    }

    state = WAIT_MODE;
    printMenu();

    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_ERROR;

    Serial.println("SMS or call failed.");

    return;
}

    if (strcmp(line, ">") == 0)
    {
        if (smsState == SMS_WAIT_PROMPT)
        {
            modem.print(smsText);
            modem.write(26);

            smsState = SMS_WAIT_RESULT;
        }

        return;
    }
    if (strncmp(line,"+CMGS:",6)==0)
    {
        Serial.print("Message reference: ");
        Serial.println(line+7);
        return;
    }

    if (strncmp(line, "+CPBR:", 6) == 0)
    {
        processPhonebook(line);
        return;
    }

    // Normal command response
    if (atCommand.active) {
        size_t len = strlen(line);
        if (atCommand.length + len + 2 < sizeof(atCommand.response)) {
            strcpy(&atCommand.response[atCommand.length], line);
            atCommand.length += len;
            atCommand.response[atCommand.length++] = '\n';
            atCommand.response[atCommand.length] = '\0';
        }
    }
}
void SIM800::processSerial()
{
    while (modem.available()) {
        char c = modem.read();

        if (c == '>' && smsState == SMS_WAIT_PROMPT) {
            processLine(">");
            continue;
        }

        if (c == '\r')
            continue;

        if (c == '\n') {
            if (linePos) {
                lineBuffer[linePos] = 0;
                processLine(lineBuffer);
                linePos = 0;
            }
        } else if (linePos < sizeof(lineBuffer) - 1) {
            lineBuffer[linePos++] = c;
        }
    }
}
bool SIM800::readSMS()
{
    if(atCommand.active)
        return false;
    smsState = SMS_READING;
    return sendAT("AT+CMGF=1",3000);

}
bool SIM800::sendAT(const char *cmd, uint32_t timeout)
{
    if (atCommand.active) {
        Serial.println("AT command busy");
        return false; // Another command is running
    }
    atCommand.active = true;
    atCommand.finished = false;
    atCommand.result = AT_TIMEOUT;
    atCommand.length = 0;
    atCommand.response[0] = '\0';
    atCommand.startTime = millis();
    atCommand.timeout = timeout;
    Serial.print(">> ");
    Serial.println(cmd);
    modem.println(cmd);
    return true;
}
void SIM800::checkATTimeout()
{
    if (!atCommand.active)
        return;
    if (millis() - atCommand.startTime > atCommand.timeout) {
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_TIMEOUT;
        Serial.println("AT command timeout");
    }
}
bool SIM800::validNumber(const char *num)
{
    if (num == nullptr)
        return false;
    if (num[0] != '+')
        return false;
    if (strlen(num) < 2)
        return false;
    for (size_t i = 1; num[i] != '\0'; i++) {
        if (!isdigit((unsigned char) num[i]))
            return false;
    }
    return true;
}
bool SIM800::atFinished()
{
    return atCommand.finished;
}
ATResult SIM800::atResult()
{
    return atCommand.result;
}
void SIM800::handleDebugInput()
{
    if (!Serial.available())
        return;

    Serial.println();
    Serial.println("===== DEBUG MODE =====");
    Serial.println("Type any AT command.");
    Serial.println("Example: AT+CSQ");
    Serial.println("Type EXIT to leave.");
    Serial.println("======================");

    char input[64];

    size_t len =
        Serial.readBytesUntil('\n',
                              input,
                              sizeof(input)-1);

    input[len] = '\0';


    while (len > 0 &&
           (input[len-1]=='\r' ||
            input[len-1]=='\n'))
    {
        input[--len]='\0';
    }


    if (strcasecmp(input,"EXIT")==0)
    {
        debugMode = false;

        Serial.println("Leaving debug mode.");
        Serial.println("Returning to normal mode.");

        return;
    }

    sendAT(input,10000);
}
ModemState SIM800::getModemState()
{
    return state;
}
SMSState SIM800::getSMSState()
{
    return smsState;
}

String ucs2ToUtf8(String hex)
{
    String out = "";

    for (int i = 0; i + 3 < hex.length(); i += 4)
    {
        uint16_t c = (strtol(hex.substring(i, i + 4).c_str(), NULL, 16));

        if (c < 0x80)
        {
            out += char(c);
        }
        else if (c < 0x800)
        {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 0x3F));
        }
        else
        {
            out += char(0xE0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }

    return out;
}

bool isUCS2(String s)
{
    if (s.length() % 4 != 0)
        return false;

    for (int i = 0; i < s.length(); i++)
    {
        if (!isxdigit(s[i]))
            return false;
    }

    return true;
}

bool SIM800::selectPhonebook(const char *storage)
{
    if (atCommand.active)
        return false;

    char cmd[32];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CPBS=\"%s\"",
        storage
    );

    return sendAT(cmd, 3000);
}


bool SIM800::writePhonebook(
    uint8_t index,
    const char *number,
    const char *name)
{
    if (!validNumber(number))
        return false;

    if (atCommand.active)
        return false;

    char cmd[128];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CPBW=%u,\"%s\",145,\"%s\"",
        index,
        number,
        name
    );

    phonebookState = PB_WAIT_WRITE;

    return sendAT(cmd, 5000);
}


bool SIM800::deletePhonebook(uint8_t index)
{
    if (atCommand.active)
        return false;

    char cmd[32];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CPBW=%u",
        index
    );

    phonebookState = PB_WAIT_DELETE;

    return sendAT(cmd, 5000);
}

bool SIM800::readPhonebook(
    uint8_t index,
    PhonebookEntry &entry)
{
    if (atCommand.active)
        return false;

    memset(&entry, 0, sizeof(entry));

    lastPhonebookEntry = entry;

    phonebookIndex = index;

    phonebookState = PB_WAIT_STORAGE;

    return selectPhonebook("SM");
}



void SIM800::processPhonebook(const char *line)
{
    PhonebookEntry entry;
    memset(&entry, 0, sizeof(entry));

    // Expected:
    // +CPBR: 1,"+21612345678",145,"Alice"

    if (strncmp(line, "+CPBR:", 6) != 0)
        return;

    const char *p = line + 6;

    // Skip spaces
    while (*p == ' ')
        p++;

    // --------------------------------------------------------
    // Index
    // --------------------------------------------------------

    char *endPtr;

    long index = strtol(p, &endPtr, 10);

    if (endPtr == p)
    {
        Serial.println("Invalid phonebook index.");
        return;
    }

    entry.index = (uint16_t)index;

    p = endPtr;

    // Skip comma/spaces
    while (*p == ' ' || *p == ',')
        p++;

    // --------------------------------------------------------
    // Phone number
    // --------------------------------------------------------

    if (*p != '"')
    {
        Serial.println("Invalid phonebook number.");
        return;
    }

    p++;

    const char *numberStart = p;
    const char *numberEnd = strchr(numberStart, '"');

    if (!numberEnd)
    {
        Serial.println("Invalid phonebook number.");
        return;
    }

    size_t numberLen = numberEnd - numberStart;

    if (numberLen >= sizeof(entry.number))
        numberLen = sizeof(entry.number) - 1;

    memcpy(
        entry.number,
        numberStart,
        numberLen
    );

    entry.number[numberLen] = '\0';

    p = numberEnd + 1;

    // --------------------------------------------------------
    // Number type
    // --------------------------------------------------------

    while (*p == ' ' || *p == ',')
        p++;

    long type = strtol(p, &endPtr, 10);

    if (endPtr == p)
    {
        Serial.println("Invalid phonebook type.");
        return;
    }

    entry.type = (int)type;

    p = endPtr;

    // Skip comma/spaces
    while (*p == ' ' || *p == ',')
        p++;

    // --------------------------------------------------------
    // Contact name
    // --------------------------------------------------------

    if (*p != '"')
    {
        Serial.println("Invalid phonebook name.");
        return;
    }

    p++;

    const char *nameStart = p;
    const char *nameEnd = strchr(nameStart, '"');

    if (!nameEnd)
    {
        Serial.println("Invalid phonebook name.");
        return;
    }

    size_t nameLen = nameEnd - nameStart;

    if (nameLen >= sizeof(entry.name))
        nameLen = sizeof(entry.name) - 1;

    memcpy(
        entry.name,
        nameStart,
        nameLen
    );

    entry.name[nameLen] = '\0';

    // --------------------------------------------------------
    // Save last entry
    // --------------------------------------------------------

    lastPhonebookEntry = entry;

    // --------------------------------------------------------
    // VIEW CONTACTS
    // --------------------------------------------------------

    if (phonebookMenu == PB_MENU_VIEW)
    {
        if (phonebookEntryCount < 20)
        {
            phonebookEntries[phonebookEntryCount] = entry;
            phonebookEntryCount++;
        }

        printPhonebookEntry(entry);

        return;
    }

    // --------------------------------------------------------
    // SEARCH CONTACT
    // --------------------------------------------------------

    if (phonebookMenu == PB_MENU_SEARCH)
    {
        if (strcasecmp(
                entry.name,
                phonebookName) == 0)
        {
            Serial.println();
            Serial.println("Contact found:");

            printPhonebookEntry(entry);

            lastPhonebookEntry = entry;
        }

        return;
    }

    // --------------------------------------------------------
    // CALL CONTACT
    // --------------------------------------------------------

    if (phonebookMenu == PB_MENU_CALL)
    {
        Serial.println();
        Serial.println("Contact:");

        printPhonebookEntry(entry);

        Serial.print("Calling ");
        Serial.println(entry.number);

        phoneNumber = entry.number;

        phonebookMenu = PB_MENU_MAIN;
        phonebookState = PB_IDLE;

        dial(phoneNumber.c_str());

        return;
    }

    // --------------------------------------------------------
    // SINGLE CONTACT READ
    // --------------------------------------------------------

    if (phonebookState == PB_WAIT_READ)
    {
        Serial.println();
        Serial.println("Contact:");

        printPhonebookEntry(entry);
    }
}



bool SIM800::listPhonebook()
{
    if (atCommand.active)
        return false;

    phonebookEntryCount = 0;

    phonebookState = PB_WAIT_STORAGE;

    return selectPhonebook("SM");
}

/*bool SIM800::findPhonebook(const char *name,
                           PhonebookEntry &entry)
{
    // This needs to be implemented using the
    // multi-entry CPBR response handling.
    return false;
}*/
void SIM800::printPhonebookMenu()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("          PHONEBOOK");
    Serial.println("================================");

    Serial.println("1. Add Contact");
    Serial.println("2. View Contacts");
    Serial.println("3. Delete Contact");
    Serial.println("4. Search Contact");
    Serial.println("5. Call Contact");
    Serial.println("6. Back");

    Serial.println();
    Serial.print("Select: ");
}
void SIM800::printPhonebookEntry(
    const PhonebookEntry &entry)
{
    Serial.print("[");

    Serial.print(entry.index);

    Serial.print("] ");

    Serial.print(entry.name);

    Serial.print(" : ");

    Serial.println(entry.number);
}

void SIM800::handlePhonebookInput()
{
    if (!Serial.available())
        return;

    String input = Serial.readStringUntil('\n');
    input.trim();

    switch (phonebookMenu)
    {
        // ====================================================
        // MAIN PHONEBOOK MENU
        // ====================================================

        case PB_MENU_MAIN:

            if (input == "1")
            {
                Serial.println();
                Serial.println("--- ADD CONTACT ---");

                Serial.println("Enter name:");

                phonebookMenu = PB_MENU_ADD_NAME;
            }

            else if (input == "2")
            {
                Serial.println();
                Serial.println("--- CONTACTS ---");

                phonebookMenu = PB_MENU_VIEW;

                phonebookEntryCount = 0;

                if (!listPhonebook())
                {
                    Serial.println("Unable to read phonebook.");
                    phonebookMenu = PB_MENU_MAIN;
                    printPhonebookMenu();
                }
            }

            else if (input == "3")
            {
                Serial.println();
                Serial.println("--- DELETE CONTACT ---");
                Serial.println("Enter contact index:");

                phonebookMenu = PB_MENU_DELETE;
            }

            else if (input == "4")
            {
                Serial.println();
                Serial.println("--- SEARCH CONTACT ---");
                Serial.println("Enter name:");

                phonebookMenu = PB_MENU_SEARCH;
            }

            else if (input == "5")
            {
                Serial.println();
                Serial.println("--- CALL CONTACT ---");
                Serial.println("Enter contact index:");

                phonebookMenu = PB_MENU_CALL;
            }

            else if (input == "6")
            {
                phonebookMenu = PB_MENU_MAIN;
                state = WAIT_MODE;

                printMenu();
            }

            else
            {
                Serial.println("Invalid option.");
                printPhonebookMenu();
            }

            break;


        // ====================================================
        // ADD CONTACT - NAME
        // ====================================================

        case PB_MENU_ADD_NAME:

            if (input.length() == 0)
            {
                Serial.println("Name cannot be empty.");
                Serial.println("Enter name:");
                break;
            }

            input.toCharArray(
                phonebookName,
                sizeof(phonebookName)
            );

            Serial.println("Enter phone number:");
            Serial.println("Example: +21612345678");

            phonebookMenu = PB_MENU_ADD_NUMBER;

            break;


        // ====================================================
        // ADD CONTACT - NUMBER
        // ====================================================

        case PB_MENU_ADD_NUMBER:

            input.toCharArray(
                phonebookNumber,
                sizeof(phonebookNumber)
            );

            if (!validNumber(phonebookNumber))
            {
                Serial.println("Invalid phone number.");
                Serial.println("Example: +21612345678");
                break;
            }

            Serial.println("Enter SIM contact index:");
            Serial.println("Example: 1");

            phonebookMenu = PB_MENU_ADD_INDEX;

            break;


        // ====================================================
        // ADD CONTACT - INDEX
        // ====================================================

        case PB_MENU_ADD_INDEX:
        {
            int index = input.toInt();

            if (index < 1 || index > 250)
            {
                Serial.println("Invalid index.");
                Serial.println("Enter a number between 1 and 250:");
                break;
            }

            phonebookIndex = index;

            if (writePhonebook(
                    phonebookIndex,
                    phonebookNumber,
                    phonebookName))
            {
                Serial.println("Saving contact...");
            }
            else
            {
                Serial.println("Unable to save contact.");

                phonebookMenu = PB_MENU_MAIN;
                printPhonebookMenu();
            }

            break;
        }


        // ====================================================
        // DELETE
        // ====================================================

        case PB_MENU_DELETE:
        {
            int index = input.toInt();

            if (index < 1 || index > 250)
            {
                Serial.println("Invalid index.");
                break;
            }

            phonebookIndex = index;

            if (deletePhonebook(phonebookIndex))
            {
                Serial.println("Deleting contact...");
            }
            else
            {
                Serial.println("Unable to delete contact.");

                phonebookMenu = PB_MENU_MAIN;
                printPhonebookMenu();
            }

            break;
        }


        // ====================================================
        // SEARCH
        // ====================================================

        case PB_MENU_SEARCH:

            input.toCharArray(
                phonebookName,
                sizeof(phonebookName)
            );

            Serial.println("Searching...");

            /*if (!findPhonebook(
                    phonebookName,
                    lastPhonebookEntry))
            {
                Serial.println("Unable to search phonebook.");
                phonebookMenu = PB_MENU_MAIN;
                printPhonebookMenu();
            }
*/
            break;


        // ====================================================
        // CALL CONTACT
        // ====================================================

        case PB_MENU_CALL:
        {
            int index = input.toInt();

            if (index < 1 || index > 250)
            {
                Serial.println("Invalid index.");
                break;
            }

            phonebookIndex = index;

            PhonebookEntry entry;

            if (readPhonebook(phonebookIndex, entry))
            {
                Serial.println("Reading contact...");
            }
            else
            {
                Serial.println("Unable to read contact.");
                phonebookMenu = PB_MENU_MAIN;
                printPhonebookMenu();
            }

            break;
        }


        default:
            phonebookMenu = PB_MENU_MAIN;
            printPhonebookMenu();
            break;
    }
}
