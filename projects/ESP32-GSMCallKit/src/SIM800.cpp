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
bool SIM800::sendSMS(const char *number,const char *message)
{

    if(smsState!=SMS_IDLE)
        return false;

    if(!validNumber(number))
        return false;

    strncpy(smsNumber,number,sizeof(smsNumber)-1);
    smsNumber[sizeof(smsNumber)-1]='\0';

    strncpy(smsText,message,sizeof(smsText)-1);
    smsText[sizeof(smsText)-1]='\0';

    smsState=SMS_WAIT_TEXTMODE;

    Serial.println("Starting SMS...");

    return sendAT("AT+CMGF=1",3000);
}
bool SIM800::sendUSSD(String code)
{
    char cmd[40];

    snprintf(cmd,
             sizeof(cmd),
             "AT+CUSD=1,\"%s\"",
             code.c_str());

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
    Serial.println("CALL     - Voice call");
    Serial.println("SMS      - Send SMS");
    Serial.println("USSD     - Send USSD");
    Serial.println("READSMS  - Read SMS");
    Serial.println("DEBUG    - AT debug");
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
            sendAT("AT+CMGL=\"ALL\"",5000);
            return;

        default:
            break;
        }

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


    if (strcmp(line, "ERROR") == 0) {
        smsState = SMS_IDLE;
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
