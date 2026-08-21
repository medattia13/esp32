#include "SIM800.h"

#include <ctype.h>
#include <string.h>

#define SIM800_RX 16
#define SIM800_TX 17

constexpr uint32_t MODEM_BAUD = 9600;

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------

SIM800::SIM800()
    : modem(2)
    , phonebook(*this)

    , modemState(ModemState::BOOTING)
    , callState(CallState::IDLE)
    , smsState(SMSState::SMS_IDLE)
    , ussdState(USSDState::USSD_IDLE)
    , uiState(UIState::MAIN_MENU)

    , modemInitStep(0)
    , recoveryStep(0)
    , recoveryStart(0)
    , recoveryAttempts(0)

    , linePos(0)

    , phoneNumber("")
    , message("")

    , smsReadIndex(0)
    , smsReadingMessage(false)

    , bootStart(0)
    , bootStep(0)

    , callStartTime(0)
{
    lineBuffer[0] = '\0';

    callerNumber[0] = '\0';

    smsNumber[0] = '\0';
    smsText[0] = '\0';

    smsReadStatus[0] = '\0';
    smsReadSender[0] = '\0';
    smsReadDate[0] = '\0';
}


// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

void SIM800::begin()
{
    modem.begin(
        MODEM_BAUD,
        SERIAL_8N1,
        SIM800_RX,
        SIM800_TX
        );

    phonebook.begin();

    Serial.println();
    Serial.println("SIM800 starting...");

    bootStart = millis();
}

void SIM800::update()
{
    processSerial();
    checkATTimeout();

    updateModem();

    // Don't allow the other state machines to operate
    // until the modem is fully ready.
    if (modemState != ModemState::READY)
        return;

    updateCall();
    updateUI();
}


// -----------------------------------------------------------------------------
// AT commands
// -----------------------------------------------------------------------------

bool SIM800::sendAT(const char *cmd, uint32_t timeout)
{
    if (atCommand.active)
    {
        Serial.println("AT command busy");
        return false;
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

    if (millis() - atCommand.startTime > atCommand.timeout)
    {
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_TIMEOUT;

        Serial.println("AT command timeout");
    }
}
void SIM800::finishAT(ATResult result)
{
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = result;
}

bool SIM800::atFinished()
{
    return atCommand.finished;
}

ATResult SIM800::atResult()
{
    return atCommand.result;
}

// -----------------------------------------------------------------------------
// Modem / Call / SMS / USSD public API
// -----------------------------------------------------------------------------

bool SIM800::dial(const char *number)
{
    if (!validNumber(number))
        return false;

    if (modemState != ModemState::READY)
        return false;

    if (callState != CallState::IDLE)
        return false;

    if (atCommand.active)
        return false;

    char cmd[48];

    snprintf(cmd, sizeof(cmd), "ATD%s;", number);

    Serial.print("Calling ");
    Serial.println(number);

    if (!sendAT(cmd, 7000))
        return false;

    callStartTime = millis();
    callState = CallState::DIALING;

    return true;
}

bool SIM800::hangup()
{
    if (atCommand.active)
    {
        atCommand.active = false;
        atCommand.finished = false;
    }
/*maybe use this code instead because hand forces at
    if (callState == CallState::IDLE)
        return false;

    if (atCommand.active)
        return false;
*/
    return sendAT("ATH", 5000);
}

bool SIM800::sendSMS(const char *number, const char *text)
{
    if (smsState != SMSState::SMS_IDLE)
    {
        Serial.println("SMS busy");
        return false;
    }

    if (!validNumber(number))
    {
        Serial.println("Invalid phone number");
        return false;
    }

    if (atCommand.active)
        return false;

    strncpy(
        smsNumber,
        number,
        sizeof(smsNumber) - 1
        );

    smsNumber[sizeof(smsNumber) - 1] = '\0';

    strncpy(
        smsText,
        text,
        sizeof(smsText) - 1
        );

    smsText[sizeof(smsText) - 1] = '\0';

    Serial.println("Starting SMS...");

    if (!sendAT("AT+CMGF=1", 3000))
    {
        Serial.println("Could not start AT+CMGF=1");
        return false;
    }

    smsState = SMSState::SMS_WAIT_TEXTMODE;

    return true;
}
bool SIM800::readSMS()
{
    if (smsState != SMSState::SMS_IDLE)
    {
        Serial.println("SMS reader busy.");
        return false;
    }

    if (atCommand.active)
    {
        Serial.println("AT command busy.");
        return false;
    }

    smsReadIndex = 0;

    smsReadStatus[0] = '\0';
    smsReadSender[0] = '\0';
    smsReadDate[0] = '\0';

    smsReadingMessage = false;

    Serial.println();
    Serial.println("Reading SMS messages...");

    /*
     * Put modem into text mode first.
     */
    if (!sendAT("AT+CMGF=1", 3000))
        return false;

    smsState = SMSState::SMS_READING;

    return true;
}


bool SIM800::deleteSMS(uint8_t index)
{
    if (smsState != SMSState::SMS_IDLE)
        return false;

    if (atCommand.active)
        return false;

    char cmd[32];

    snprintf(cmd, sizeof(cmd), "AT+CMGD=%u", index);

    return sendAT(cmd, 5000);
}
// -----------------------------------------------------------------------------
// SMS line handling
// -----------------------------------------------------------------------------


bool SIM800::handleSMSLine(const char *line)
{
    // ========================================================
    // SMS SEND PROMPT
    // ========================================================

    if (strcmp(line, ">") == 0)
    {
        if (smsState == SMSState::SMS_WAIT_PROMPT)
        {
            modem.print(smsText);
            modem.write(26);

            smsState = SMSState::SMS_WAIT_RESULT;
        }

        return true;
    }


    // ========================================================
    // SMS SEND MESSAGE REFERENCE
    // ========================================================

    if (strncmp(line, "+CMGS:", 6) == 0)
    {
        Serial.print("Message reference: ");
        Serial.println(line + 7);

        return true;
    }


    // ========================================================
    // SMS LIST HEADER
    // ========================================================

    if (strncmp(line, "+CMGL:", 6) == 0)
    {
        if (smsState != SMSState::SMS_WAIT_LIST)
            return false;

        /*
         * Parse:
         *
         * +CMGL: index,"status","number","","date"
         */
        if (!parseSMSHeader(line))
        {
            Serial.println("Unable to parse SMS header.");

            smsReadingMessage = false;

            return true;
        }

        printSMSHeader();

        /*
         * The next modem line is the message body.
         */
        smsReadingMessage = true;

        return true;
    }


    // ========================================================
    // SMS MESSAGE BODY
    // ========================================================

    if (smsState == SMSState::SMS_WAIT_LIST &&
        smsReadingMessage)
    {
        Serial.print("Message   : ");
        Serial.println(line);

        Serial.println("================================");

        smsReadingMessage = false;

        return true;
    }


    // ========================================================
    // SMS COMMAND RESPONSE
    // ========================================================

    if (strcmp(line, "OK") == 0)
    {
        switch (smsState)
        {
            // ------------------------------------------------
            // Finished AT+CMGF=1 while sending SMS
            // ------------------------------------------------

        case SMSState::SMS_WAIT_TEXTMODE:
        {
            finishAT(AT_OK);
            smsState = SMSState::SMS_WAIT_CHARSET;

            if (!sendAT("AT+CSCS=\"GSM\"", 3000))
            {
                smsState = SMSState::SMS_IDLE;

                returnToMainMenu();
            }

            return true;
        }


            // ------------------------------------------------
            // Finished AT+CSCS
            // ------------------------------------------------

        case SMSState::SMS_WAIT_CHARSET:
        {
            char cmd[64];

            snprintf(
                cmd,
                sizeof(cmd),
                "AT+CMGS=\"%s\"",
                smsNumber
                );

            if (!sendAT(cmd, 5000))
            {
                smsState = SMSState::SMS_IDLE;

                returnToMainMenu();

                return true;
            }

            smsState = SMSState::SMS_WAIT_PROMPT;

            return true;
        }


            // ------------------------------------------------
            // SMS sent
            // ------------------------------------------------

        case SMSState::SMS_WAIT_RESULT:
        {
            smsNumber[0] = '\0';
            smsText[0] = '\0';

            smsState = SMSState::SMS_IDLE;

            Serial.println("SMS sent successfully.");

            returnToMainMenu();

            return true;
        }


            // ------------------------------------------------
            // AT+CMGF=1 finished while reading SMS
            // ------------------------------------------------

        case SMSState::SMS_READING:
{
    // AT+CMGF=1 has completed.
    finishAT(AT_OK);

    smsState = SMSState::SMS_WAIT_LIST;

    smsReadingMessage = false;

    if (!sendAT("AT+CMGL=\"ALL\"", 15000))
    {
        smsState = SMSState::SMS_IDLE;

        Serial.println("Unable to read SMS list.");

        returnToMainMenu();
    }

    return true;
}



            // ------------------------------------------------
            // Finished SMS list
            // ------------------------------------------------

        case SMSState::SMS_WAIT_LIST:
        {
            smsReadingMessage = false;

            smsReadStatus[0] = '\0';
            smsReadSender[0] = '\0';
            smsReadDate[0] = '\0';

            smsState = SMSState::SMS_IDLE;

            Serial.println();
            Serial.println("Finished reading SMS.");

            returnToMainMenu();

            return true;
        }


        default:
            break;
        }
    }


    // ========================================================
    // SMS ERROR
    // ========================================================

    if (strcmp(line, "ERROR") == 0)
    {
        if (smsState != SMSState::SMS_IDLE)
        {
            smsState = SMSState::SMS_IDLE;

            smsNumber[0] = '\0';
            smsText[0] = '\0';

            smsReadStatus[0] = '\0';
            smsReadSender[0] = '\0';
            smsReadDate[0] = '\0';

            smsReadingMessage = false;

            atCommand.active = false;
            atCommand.finished = true;
            atCommand.result = AT_ERROR;

            Serial.println("SMS operation failed.");

            returnToMainMenu();

            return true;
        }

        return false;
    }


    return false;
}


void SIM800::printSMSHeader()
{
    Serial.println();
    Serial.println("================================");

    Serial.print("SMS index : ");
    Serial.println(smsReadIndex);

    Serial.print("Status    : ");
    Serial.println(smsReadStatus);

    Serial.print("From      : ");
    Serial.println(smsReadSender);

    Serial.print("Date      : ");
    Serial.println(smsReadDate);

    Serial.println("--------------------------------");
}
bool SIM800::parseSMSHeader(const char *line)
{
    if (line == nullptr)
        return false;

    // Expected format:
    // +CMGL: index,"status","number","","date"

    int index = 0;

    char status[32] = "";
    char sender[32] = "";
    char date[32] = "";

    int parsed = sscanf(
        line,
        "+CMGL: %d,\"%31[^\"]\",\"%31[^\"]\",\"\",\"%31[^\"]\"",
        &index,
        status,
        sender,
        date
    );

    if (parsed != 4)
        return false;

    smsReadIndex = index;

    strncpy(
        smsReadStatus,
        status,
        sizeof(smsReadStatus) - 1
    );
    smsReadStatus[sizeof(smsReadStatus) - 1] = '\0';

    strncpy(
        smsReadSender,
        sender,
        sizeof(smsReadSender) - 1
    );
    smsReadSender[sizeof(smsReadSender) - 1] = '\0';

    strncpy(
        smsReadDate,
        date,
        sizeof(smsReadDate) - 1
    );
    smsReadDate[sizeof(smsReadDate) - 1] = '\0';

    return true;
}

bool SIM800::sendUSSD(const String &code)
{
    if (ussdState != USSDState::USSD_IDLE)
        return false;

    if (atCommand.active)
        return false;

    if (code.length() == 0)
        return false;

    char cmd[64];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CUSD=1,\"%s\"",
        code.c_str()
        );

    if (!sendAT(cmd, 10000))
        return false;

    ussdState = USSDState::USSD_WAIT_RESULT;
    uiState = UIState::USSD_WAIT_RESULT;

    return true;
}

// -----------------------------------------------------------------------------
// Serial input from modem
// -----------------------------------------------------------------------------

void SIM800::processSerial()
{
    while (modem.available())
    {
        char c = modem.read();

        // SMS prompt isn't terminated by newline.
        if (c == '>' &&
            smsState == SMSState::SMS_WAIT_PROMPT)
        {
            processLine(">");

            continue;
        }

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            if (linePos > 0)
            {
                lineBuffer[linePos] = '\0';

                processLine(lineBuffer);

                linePos = 0;
            }

            continue;
        }

        if (linePos < sizeof(lineBuffer) - 1)
        {
            lineBuffer[linePos++] = c;
        }
        else
        {
            // Prevent stale data after an oversized modem line.
            linePos = 0;
            lineBuffer[0] = '\0';
        }
    }
}

void SIM800::processLine(const char *line)
{
    Serial.print("<< ");
    Serial.println(line);

    if (handleCallLine(line))
        return;

    if (handleSMSLine(line))
        return;

    if (handleUSSDLine(line))
        return;

    if (handlePhonebookLine(line))
        return;

    handleATResponse(line);
}

// -----------------------------------------------------------------------------
// Call line handling
// -----------------------------------------------------------------------------

bool SIM800::handleCallLine(const char *line)
{
    if (strncmp(line, "+CLIP:", 6) == 0)
    {
        handleCallerID(line);
        return true;
    }

    if (strcmp(line, "RING") == 0)
    {
        callState = CallState::INCOMING_CALL;

        Serial.println();
        Serial.println("========== INCOMING CALL ==========");

        if (callerNumber[0] != '\0')
        {
            Serial.print("From: ");
            Serial.println(callerNumber);
        }
        else
        {
            Serial.println("From: Unknown");
        }

        Serial.println("A = Answer");
        Serial.println("H = Reject");
        Serial.println("===================================");

        return true;
    }

    if (strcmp(line, "CONNECT") == 0)
    {
        callState = CallState::IN_CALL;

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_OK;

        return true;
    }

    if (strcmp(line, "NO CARRIER") == 0 ||
        strcmp(line, "BUSY") == 0 ||
        strcmp(line, "NO ANSWER") == 0 ||
        strcmp(line, "NO DIALTONE") == 0)
    {
        if (strcmp(line, "BUSY") == 0)
            Serial.println("Call failed: busy.");
        else if (strcmp(line, "NO ANSWER") == 0)
            Serial.println("Call not answered.");
        else if (strcmp(line, "NO DIALTONE") == 0)
            Serial.println("Call failed: no dial tone.");
        else
            Serial.println("Call ended.");

        callState = CallState::IDLE;
        callerNumber[0] = '\0';

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;

        returnToMainMenu();

        return true;
    }

    return false;
}

void SIM800::handleCallerID(const char *line)
{
    const char *start = strchr(line, '"');

    if (!start)
        return;

    start++;

    const char *end = strchr(start, '"');

    if (!end)
        return;

    size_t len = end - start;

    if (len >= sizeof(callerNumber))
        len = sizeof(callerNumber) - 1;

    memcpy(
        callerNumber,
        start,
        len
        );

    callerNumber[len] = '\0';

    Serial.print("Incoming call from: ");
    Serial.println(callerNumber);
}



// -----------------------------------------------------------------------------
// USSD line handling
// -----------------------------------------------------------------------------

bool SIM800::handleUSSDLine(const char *line)
{
    if (strncmp(line, "+CUSD:", 6) != 0)
        return false;

    Serial.println("USSD reply:");
    Serial.println(line);

    ussdState = USSDState::USSD_IDLE;

    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_OK;

    returnToMainMenu();

    return true;
}

// -----------------------------------------------------------------------------
// Phonebook line handling
// -----------------------------------------------------------------------------

bool SIM800::handlePhonebookLine(const char *line)
{
    if (strncmp(line, "+CPBR:", 6) != 0)
        return false;

    phonebook.processLine(line);

    return true;
}

// -----------------------------------------------------------------------------
// Generic AT response
// -----------------------------------------------------------------------------

bool SIM800::handleATResponse(const char *line)
{
    if (strcmp(line, "OK") == 0)
    {
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_OK;

        if (phonebook.isBusy())
        {
            phonebook.onOk();
            return true;
        }

        return true;
    }

    if (strcmp(line, "ERROR") == 0)
    {
        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_ERROR;

        if (phonebook.isBusy())
        {
            phonebook.onError();
            return true;
        }

        return true;
    }

    if (atCommand.active)
    {
        size_t len = strlen(line);

        if (atCommand.length + len + 2 <
            sizeof(atCommand.response))
        {
            strcpy(
                &atCommand.response[atCommand.length],
                line
                );

            atCommand.length += len;

            atCommand.response[atCommand.length++] = '\n';
            atCommand.response[atCommand.length] = '\0';
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// Call state machine
// -----------------------------------------------------------------------------

void SIM800::updateCall()
{
    switch (callState)
    {
    case CallState::IDLE:
        break;

    case CallState::DIALING:
        updateDialingCall();
        break;

    case CallState::INCOMING_CALL:
        updateIncomingCall();
        break;

    case CallState::IN_CALL:
        updateActiveCall();
        break;
    }
}

void SIM800::updateDialingCall()
{
    if (millis() - callStartTime >= DIAL_TIMEOUT)
    {
        Serial.println("Call timeout.");

        if (!hangup())
        {
            callState = CallState::IDLE;
            returnToMainMenu();
        }

        return;
    }

    if (!Serial.available())
        return;

    char c = Serial.read();

    if (c == 'h' || c == 'H')
    {
        if (!hangup())
            Serial.println("Unable to send hangup.");
    }
}

void SIM800::updateIncomingCall()
{
    if (!Serial.available())
        return;

    char c = Serial.read();

    if (c == 'a' || c == 'A')
    {
        Serial.println("Answering call...");

        if (!sendAT("ATA", 5000))
            Serial.println("Unable to answer call.");
    }
    else if (c == 'h' || c == 'H')
    {
        Serial.println("Rejecting call...");

        if (!sendAT("ATH", 5000))
            Serial.println("Unable to reject call.");
    }
}

void SIM800::updateActiveCall()
{
    if (!Serial.available())
        return;

    char c = Serial.read();

    if (c == 'h' || c == 'H')
    {
        Serial.println("Hanging up...");

        if (!hangup())
            Serial.println("Unable to send hangup.");
    }
}

// -----------------------------------------------------------------------------
// Modem state machine
// -----------------------------------------------------------------------------

void SIM800::updateModem()
{
    switch (modemState)
    {
    case ModemState::BOOTING:
        updateModemBoot();
        break;

    case ModemState::INITIALIZING:
        updateModemInitialization();
        break;

    case ModemState::READY:
        break;

    case ModemState::MODEM_ERROR:
        startModemRecovery();
        break;

    case ModemState::MODEM_RECOVERING:
        updateModemRecovery();
        break;
    }
}

void SIM800::updateModemBoot()
{
    if (millis() - bootStart < MODEM_BOOT_TIME)
        return;

    if (!atCommand.active && !atCommand.finished)
    {
        const char *cmd = nullptr;

        switch (bootStep)
        {
        case 0:
            cmd = "AT";
            break;

        case 1:
            cmd = "ATE0";
            break;

        default:
            modemInitStep = 0;
            modemState = ModemState::INITIALIZING;
            return;
        }

        if (!sendAT(cmd, 3000))
            return;
    }

    if (!atFinished())
        return;

    if (atResult() != AT_OK)
    {
        Serial.println("SIM800 boot failed.");

        atCommand.finished = false;
        modemState = ModemState::MODEM_ERROR;

        return;
    }

    atCommand.finished = false;
    bootStep++;
}

void SIM800::updateModemInitialization()
{
    if (atCommand.active)
        return;

    if (atCommand.finished)
    {
        if (atResult() != AT_OK)
        {
            Serial.println("SIM800 initialization failed.");

            atCommand.finished = false;
            modemState = ModemState::MODEM_ERROR;

            return;
        }

        atCommand.finished = false;
        modemInitStep++;
    }

    const char *cmd = nullptr;

    switch (modemInitStep)
    {
    case 0:
        cmd = "AT+CPIN?";
        break;

    case 1:
        cmd = "AT+CREG?";
        break;

    case 2:
        cmd = "AT+CSQ";
        break;

    case 3:
        cmd = "AT+CLIP=1";
        break;

    default:
        Serial.println("SIM800 ready.");

        modemState = ModemState::READY;
        recoveryAttempts = 0;

        returnToMainMenu();

        return;
    }

    if (!sendAT(cmd, 3000))
        modemState = ModemState::MODEM_ERROR;
}

// -----------------------------------------------------------------------------
// Modem recovery
// -----------------------------------------------------------------------------
void SIM800::startModemRecovery()
{
    if (modemState == ModemState::MODEM_RECOVERING)
        return;

    if (recoveryAttempts >= MAX_RECOVERY_ATTEMPTS)
    {
        Serial.println("SIM800 recovery failed permanently.");
        return;
    }

    Serial.println();
    Serial.println("SIM800 recovery starting...");

    atCommand.active = false;
    atCommand.finished = false;

    callState = CallState::IDLE;
    smsState = SMSState::SMS_IDLE;
    ussdState = USSDState::USSD_IDLE;

    callerNumber[0] = '\0';

    recoveryStep = 0;
    recoveryStart = millis();

    recoveryAttempts++;

    modemState = ModemState::MODEM_RECOVERING;
}

void SIM800::updateModemRecovery()
{
    switch (recoveryStep)
    {
    case 0:
        Serial.println("Attempting modem restart...");

        modem.println("AT+CFUN=1,1");
        atCommand.active = false;
        atCommand.finished = false;
        recoveryStart = millis();
        recoveryStep = 1;

        break;

    case 1:
        if (millis() - recoveryStart < MODEM_BOOT_TIME)
            return;

        Serial.println("Retrying modem communication.");

        bootStep = 0;
        modemInitStep = 0;

        bootStart = millis();

        atCommand.active = false;
        atCommand.finished = false;

        modemState = ModemState::BOOTING;

        break;

    default:
        modemState = ModemState::MODEM_ERROR;
        break;
    }
}

// -----------------------------------------------------------------------------
// UI state machine
// -----------------------------------------------------------------------------

void SIM800::updateUI()
{
    switch (uiState)
    {
    case UIState::MAIN_MENU:
        updateUIMainMenu();
        break;

    case UIState::CALL_NUMBER:
        updateUICallNumber();
        break;

    case UIState::SMS_NUMBER:
        updateUISMSNumber();
        break;

    case UIState::SMS_MESSAGE:
        updateUISMSMessage();
        break;

    case UIState::USSD_INPUT:
        updateUIUSSDInput();
        break;

    case UIState::USSD_WAIT_RESULT:
        break;

    case UIState::PHONEBOOK:
        phonebook.update();
        break;

    case UIState::DEBUG_MODE:
        handleDebugInput();
        break;
    }
}

void SIM800::updateUIMainMenu()
{
    if (!Serial.available())
        return;

    String input = Serial.readStringUntil('\n');

    input.trim();
    input.toUpperCase();

    if (input == "CALL")
    {
        Serial.println("Enter phone number:");
        Serial.println("Example: +21612345678");

        uiState = UIState::CALL_NUMBER;
    }
    else if (input == "SMS")
    {
        Serial.println("Phone number:");

        uiState = UIState::SMS_NUMBER;
    }
    else if (input == "USSD")
    {
        Serial.println("Enter USSD code:");

        uiState = UIState::USSD_INPUT;
    }
    else if (input == "READSMS")
    {
        if (!readSMS())
            Serial.println("Unable to read SMS.");
    }
    else if (input == "DEBUG")
    {
        Serial.println("Entering DEBUG mode.");
        Serial.println("Type EXIT to leave.");

        uiState = UIState::DEBUG_MODE;
    }
    else if (input == "CONTACTS")
    {
        uiState = UIState::PHONEBOOK;
        phonebook.open();
    }
    else if (input == "HANG")
    {
        if (!hangup())
            Serial.println("Unable to hang up.");
    }
    else
    {
        printMenu();
    }
}

void SIM800::updateUICallNumber()
{
    if (!Serial.available())
        return;

    String number = Serial.readStringUntil('\n');
    number.trim();

    if (dial(number.c_str()))
    {
        phoneNumber = "";

        // CallState now owns the call.
        uiState = UIState::MAIN_MENU;
    }
    else
    {
        Serial.println("Invalid phone number.");

        returnToMainMenu();
    }
}

void SIM800::updateUISMSNumber()
{
    if (!Serial.available())
        return;

    phoneNumber = Serial.readStringUntil('\n');
    phoneNumber.trim();

    if (!validNumber(phoneNumber.c_str()))
    {
        Serial.println("Invalid phone number.");

        returnToMainMenu();

        return;
    }

    Serial.println("Message:");

    uiState = UIState::SMS_MESSAGE;
}

void SIM800::updateUISMSMessage()
{
    if (!Serial.available())
        return;

    message = Serial.readStringUntil('\n');
    message.trim();

    if (!sendSMS(
            phoneNumber.c_str(),
            message.c_str()))
    {
        Serial.println("Unable to start SMS.");

        returnToMainMenu();

        return;
    }

    phoneNumber = "";
    message = "";
}

void SIM800::updateUIUSSDInput()
{
    if (!Serial.available())
        return;

    String code = Serial.readStringUntil('\n');
    code.trim();

    if (!sendUSSD(code))
    {
        Serial.println("USSD failed.");

        returnToMainMenu();

        return;
    }

    Serial.println("Waiting for USSD reply...");
}


// -----------------------------------------------------------------------------
// UI helpers
// -----------------------------------------------------------------------------

void SIM800::returnToMainMenu()
{
    uiState = UIState::MAIN_MENU;

    phonebook.begin();

    printMenu();
}

void SIM800::printMenu()
{
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

void SIM800::cleanInput(char *input)
{
    size_t len = strlen(input);

    while (len > 0 &&
           (input[len - 1] == '\r' ||
            input[len - 1] == '\n'))
    {
        input[--len] = '\0';
    }
}

void SIM800::handleDebugInput()
{
    if (!Serial.available())
        return;

    char input[64];

    size_t len = Serial.readBytesUntil(
        '\n',
        input,
        sizeof(input) - 1
        );

    input[len] = '\0';

    cleanInput(input);

    if (strcasecmp(input, "EXIT") == 0)
    {
        Serial.println("Leaving debug mode.");

        returnToMainMenu();

        return;
    }

    if (!sendAT(input, 10000))
        Serial.println("AT command busy.");
}

// -----------------------------------------------------------------------------
// Public state getters
// -----------------------------------------------------------------------------

ModemState SIM800::getModemState()const
{
    return modemState;
}

SMSState SIM800::getSMSState()const
{
    return smsState;
}
CallState SIM800::getCallState() const
{
    return callState;
}

USSDState SIM800::getUSSDState() const
{
    return ussdState;
}

UIState SIM800::getUIState() const
{
    return uiState;
}

// -----------------------------------------------------------------------------
// Validation
// -----------------------------------------------------------------------------

bool SIM800::validNumber(const char *num)
{
    if (num == nullptr)
        return false;

    if (num[0] != '+')
        return false;

    if (strlen(num) < 2)
        return false;

    for (size_t i = 1; num[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)num[i]))
            return false;
    }

    return true;
}
