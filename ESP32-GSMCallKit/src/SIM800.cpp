#include <ctype.h>
#include <string.h>
#include "SIM800.h"
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
    , hangupPending(false)
    , atOwner(ATOwner::NONE)
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

bool SIM800::sendAT(const char *cmd, uint32_t timeout, ATOwner owner)
{
    if (atCommand.active || atCommand.finished)
    {
        Serial.print("AT command busy. Owner: ");
        Serial.println(atOwnerName());
        return false;
    }

    atOwner = owner;

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

    Serial.print("AT owner: ");
    Serial.println(atOwnerName());

    return true;
}

void SIM800::checkATTimeout()
{
    if (!atCommand.active)
        return;

    if (millis() - atCommand.startTime > atCommand.timeout)
    {
        Serial.print("AT command timeout. Owner=");
        Serial.print(atOwnerName());
        Serial.print(" SMS state=");
        Serial.print((int)smsState);
        Serial.print(" Call state=");
        Serial.print((int)callState);
        Serial.print(" UI state=");
        Serial.println((int)uiState);

        atCommand.active = false;
        atCommand.finished = true;
        atCommand.result = AT_TIMEOUT;
    }
}

void SIM800::finishAT(ATResult result)
{
    if (!atCommand.active)
        return;

    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = result;
}

bool SIM800::atFinished()
{
    return atCommand.finished;
}
void SIM800::releaseAT()
{
    atCommand.active = false;
    atCommand.finished = false;
    atCommand.result = AT_TIMEOUT;
    atCommand.length = 0;
    atCommand.response[0] = '\0';

    atOwner = ATOwner::NONE;
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

if (atCommand.active || atCommand.finished)
    return false;


    hangupPending = false;
    char cmd[48];

    snprintf(cmd, sizeof(cmd), "ATD%s;", number);

    Serial.print("Calling ");
    Serial.println(number);

    if (!sendAT(cmd, 7000, ATOwner::CALL))
        return false;

    callStartTime = millis();
    callState = CallState::DIALING;

    return true;
}

bool SIM800::hangup()
{
    // Nothing to hang up.
    if (callState == CallState::IDLE)
        return false;

    // A CALL-owned AT command is currently running.
    // Do not cancel it. Queue the hangup instead.
    if (atCommand.active)
    {
        if (atOwner == ATOwner::CALL)
        {
            Serial.println(
                "Hangup requested; waiting for current call AT command."
            );

            hangupPending = true;
            return true;
        }

        Serial.print(
            "Cannot hang up: AT channel owned by "
        );
        Serial.println(atOwnerName());

        return false;
    }

    // A CALL command has finished but its result has not yet
    // been consumed by the call state machine.
    if (atCommand.finished)
    {
        if (atOwner == ATOwner::CALL)
        {
            Serial.println(
                "Hangup requested; waiting for CALL result to be consumed."
            );

            hangupPending = true;
            return true;
        }

        Serial.print(
            "Cannot hang up: AT result owned by "
        );
        Serial.println(atOwnerName());

        return false;
    }

    // AT channel is completely free.
    if (!sendAT("ATH", 5000, ATOwner::CALL))
        return false;

    return true;
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

if (atCommand.active || atCommand.finished)
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

    if (!sendAT("AT+CMGF=1", 3000,ATOwner::SMS))
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

    if (atCommand.active || atCommand.finished)

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
    if (!sendAT("AT+CMGF=1", 3000,ATOwner::SMS))
        return false;

    smsState = SMSState::SMS_READING;

    return true;
}


bool SIM800::deleteSMS(uint8_t index)
{
    if (smsState != SMSState::SMS_IDLE)
        return false;
if (atCommand.active || atCommand.finished)
    return false;


    char cmd[32];

    snprintf(cmd, sizeof(cmd), "AT+CMGD=%u", index);

    return sendAT(cmd, 5000,ATOwner::SMS);
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

        if (!parseSMSHeader(line))
        {
            Serial.println("Unable to parse SMS header.");

            smsReadingMessage = false;
            return true;
        }

        printSMSHeader();

        // The next line is the message body.
        smsReadingMessage = true;

        return true;
    }


    // ========================================================
    // IMPORTANT:
    // Handle terminal responses BEFORE SMS MESSAGE BODY.
    // Otherwise the final "OK" gets consumed as the message.
    // ========================================================

    if (strcmp(line, "OK") == 0)
    {
      if (atOwner != ATOwner::SMS || !atCommand.active)
        return false;

        switch (smsState)
        {
        // ----------------------------------------------------
        // AT+CMGF=1 finished while sending SMS
        // ----------------------------------------------------

        case SMSState::SMS_WAIT_TEXTMODE:
        {
 if (atResult() != AT_OK)
    {
        consumeAT();
        smsState = SMSState::SMS_IDLE;
        returnToMainMenu();
        return true;
    }

    consumeAT();

            smsState = SMSState::SMS_WAIT_CHARSET;

            if (!sendAT(
                    "AT+CSCS=\"GSM\"",
                    3000,
                    ATOwner::SMS))
            {
                smsState = SMSState::SMS_IDLE;
                returnToMainMenu();
            }

            return true;
        }


        // ----------------------------------------------------
        // AT+CSCS finished
        // ----------------------------------------------------

        case SMSState::SMS_WAIT_CHARSET:
        {
            // The previous command has completed.
             if (atResult() != AT_OK)
    {
        consumeAT();
        smsState = SMSState::SMS_IDLE;
        returnToMainMenu();
        return true;
    }

    consumeAT();

            char cmd[64];

            snprintf(
                cmd,
                sizeof(cmd),
                "AT+CMGS=\"%s\"",
                smsNumber
            );

            if (!sendAT(
                    cmd,
                    5000,
                    ATOwner::SMS))
            {
                smsState = SMSState::SMS_IDLE;
                returnToMainMenu();
                return true;
            }

            smsState = SMSState::SMS_WAIT_PROMPT;

            return true;
        }


        // ----------------------------------------------------
        // SMS send completed
        // ----------------------------------------------------

        case SMSState::SMS_WAIT_RESULT:
        {
             if (atResult() != AT_OK)
    {
        consumeAT();
        smsState = SMSState::SMS_IDLE;
        returnToMainMenu();
        return true;
    }

    consumeAT();

            smsNumber[0] = '\0';
            smsText[0] = '\0';

            smsState = SMSState::SMS_IDLE;

            Serial.println("SMS sent successfully.");

            returnToMainMenu();

            return true;
        }


        // ----------------------------------------------------
        // AT+CMGF=1 finished while reading SMS
        // ----------------------------------------------------

        case SMSState::SMS_READING:
        {
             if (atResult() != AT_OK)
    {
        consumeAT();
        smsState = SMSState::SMS_IDLE;
        returnToMainMenu();
        return true;
    }

    consumeAT();

            smsState = SMSState::SMS_WAIT_LIST;
            smsReadingMessage = false;

            if (!sendAT(
                    "AT+CMGL=\"ALL\"",
                    15000,
                    ATOwner::SMS))
            {
                smsState = SMSState::SMS_IDLE;

                Serial.println("Unable to read SMS list.");

                returnToMainMenu();
            }

            return true;
        }


        // ----------------------------------------------------
        // AT+CMGL="ALL" finished
        // ----------------------------------------------------

case SMSState::SMS_WAIT_LIST:
{
    if (atOwner != ATOwner::SMS || !atCommand.active)
        return true;

    finishAT(AT_OK);

    smsReadingMessage = false;

    smsReadStatus[0] = '\0';
    smsReadSender[0] = '\0';
    smsReadDate[0] = '\0';

    smsState = SMSState::SMS_IDLE;

    consumeAT();

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
    // SMS MESSAGE BODY
    //
    // This MUST come AFTER OK handling.
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
    // SMS ERROR
    // ========================================================

    if (strcmp(line, "ERROR") == 0)
    {
        if (smsState != SMSState::SMS_IDLE)
        {
            finishAT(AT_ERROR);

            smsState = SMSState::SMS_IDLE;

            smsNumber[0] = '\0';
            smsText[0] = '\0';

            smsReadStatus[0] = '\0';
            smsReadSender[0] = '\0';
            smsReadDate[0] = '\0';

            smsReadingMessage = false;

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

if (atCommand.active || atCommand.finished)
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

    if (!sendAT(cmd, 10000,ATOwner::USSD))
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

    // 1. Unsolicited call events
    if (handleCallLine(line))
        return;

    // 2. SMS-specific responses
    if (atOwner == ATOwner::SMS &&
        handleSMSLine(line))
        return;

    // 3. USSD-specific responses
    if (atOwner == ATOwner::USSD &&
        handleUSSDLine(line))
        return;

    // 4. Phonebook-specific responses
    if (atOwner == ATOwner::PHONEBOOK &&
        handlePhonebookLine(line))
        return;

    // 5. Generic AT transaction
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

    if (atOwner == ATOwner::CALL &&
        atCommand.active)
    {
        finishAT(AT_OK);
        consumeAT();
    }

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

    if (atOwner == ATOwner::CALL &&
        atCommand.active)
    {
        finishAT(AT_ERROR);
        consumeAT();
    }

    callState = CallState::IDLE;
    callerNumber[0] = '\0';
    hangupPending = false;

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

    if (atOwner != ATOwner::USSD ||
        !atCommand.active)
        return false;

    Serial.println("USSD reply:");
    Serial.println(line);

    finishAT(AT_OK);

    ussdState = USSDState::USSD_IDLE;

    consumeAT();

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
    // No command is waiting for a terminal response.
    if (!atCommand.active)
        return false;

    if (strcmp(line, "OK") == 0)
    {
    
        finishAT(AT_OK);

        if (atOwner == ATOwner::PHONEBOOK)
            phonebook.onOk();

        return true;
    }

    if (strcmp(line, "ERROR") == 0)
    {
        finishAT(AT_ERROR);

        if (atOwner == ATOwner::PHONEBOOK)
            phonebook.onError();

        return true;
    }

    size_t len = strlen(line);

    if (atCommand.length + len + 2 <
        sizeof(atCommand.response))
    {
        memcpy(
            &atCommand.response[atCommand.length],
            line,
            len
        );

        atCommand.length += len;

        atCommand.response[atCommand.length++] = '\n';
        atCommand.response[atCommand.length] = '\0';
    }

    return true;
}


// -----------------------------------------------------------------------------
// Call state machine
// -----------------------------------------------------------------------------

void SIM800::updateCall()
{
    // A hangup was requested while a CALL-owned AT command
    // was executing. Wait until that transaction is completely
    // consumed before sending ATH.
if (hangupPending)
{
    if (atCommand.active)
        return;

    if (atCommand.finished)
    {
        if (atOwner == ATOwner::CALL)
            consumeAT();
        else
        {
            hangupPending = false;
            return;
        }
    }

    hangupPending = false;

    if (sendAT("ATH", 5000, ATOwner::CALL))
        return;

    Serial.println("Unable to send queued hangup.");
    return;
}


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

        if (!sendAT("ATA", 5000,ATOwner::CALL))
            Serial.println("Unable to answer call.");
    }
    else if (c == 'h' || c == 'H')
    {
        Serial.println("Rejecting call...");

        if (!sendAT("ATH", 5000,ATOwner::CALL))
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

        if (!sendAT(cmd, 3000,ATOwner::MODEM))
            return;
    }

    if (!atFinished())
        return;

if (atResult() != AT_OK)
{
    Serial.println("SIM800 boot failed.");

    consumeAT();

    modemState = ModemState::MODEM_ERROR;

    return;
}

consumeAT();

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

        consumeAT();

        modemState = ModemState::MODEM_ERROR;
        return;
    }

    consumeAT();
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

    if (!sendAT(cmd, 3000,ATOwner::MODEM))
        modemState = ModemState::MODEM_ERROR;
}

// -----------------------------------------------------------------------------
// Modem recovery
// -----------------------------------------------------------------------------
void SIM800::startModemRecovery()
{
    if (recoveryAttempts >= MAX_RECOVERY_ATTEMPTS)
    {
        Serial.println("SIM800 recovery failed permanently.");
        return;
    }

    Serial.println();
    Serial.println("SIM800 recovery starting...");

    abortOperations();

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

    releaseAT();

    modem.println("AT+CFUN=1,1");

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

    releaseAT();

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

    char input[64];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    for (size_t i = 0; input[i] != '\0'; i++)
    {
        input[i] = toupper((unsigned char)input[i]);
    }

    if (strcmp(input, "CALL") == 0)
    {
        Serial.println("Enter phone number:");
        Serial.println("Example: +21612345678");

        uiState = UIState::CALL_NUMBER;
    }
    else if (strcmp(input, "SMS") == 0)
    {
        Serial.println("Phone number:");

        uiState = UIState::SMS_NUMBER;
    }
    else if (strcmp(input, "USSD") == 0)
    {
        Serial.println("Enter USSD code:");

        uiState = UIState::USSD_INPUT;
    }
    else if (strcmp(input, "READSMS") == 0)
    {
        if (!readSMS())
            Serial.println("Unable to read SMS.");
    }
    else if (strcmp(input, "DEBUG") == 0)
    {
        Serial.println("Entering DEBUG mode.");
        Serial.println("Type EXIT to leave.");

        uiState = UIState::DEBUG_MODE;
    }
    else if (strcmp(input, "CONTACTS") == 0)
    {
        uiState = UIState::PHONEBOOK;
        phonebook.open();
    }
    else if (strcmp(input, "HANG") == 0)
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
    char input[32];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    if (dial(input))
    {
        phoneNumber = "";
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
    char input[32];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    if (!validNumber(input))
    {
        Serial.println("Invalid phone number.");
        Serial.println("Returning to main menu.");

        returnToMainMenu();
        return;
    }

    phoneNumber = input;

    Serial.println("Message:");
    uiState = UIState::SMS_MESSAGE;
}


void SIM800::updateUISMSMessage()
{
    char input[161];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    if (input[0] == '\0')
    {
        Serial.println("Message cannot be empty.");
        return;
    }

    message = input;

    if (!sendSMS(
            phoneNumber.c_str(),
            message.c_str()))
    {
        Serial.println("Unable to start SMS.");

        phoneNumber = "";
        message = "";

        returnToMainMenu();
        return;
    }

    phoneNumber = "";
    message = "";
}


void SIM800::updateUIUSSDInput()
{
    char input[64];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    if (input[0] == '\0')
    {
        Serial.println("Invalid USSD code.");
        returnToMainMenu();
        return;
    }

    if (!sendUSSD(input))
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
    char input[64];

    if (!readUserLine(input, sizeof(input)))
        return;

    cleanInput(input);

    if (input[0] == '\0')
        return;

    if (strcasecmp(input, "EXIT") == 0)
    {
        Serial.println("Leaving debug mode.");

        returnToMainMenu();
        return;
    }

    if (!sendAT(input, 10000, ATOwner::DEBUG))
    {
        Serial.println("AT command busy.");
    }
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

//debugging helper
const char* SIM800::atOwnerName() 
{
    switch (atOwner)
    {
    case ATOwner::NONE:
        return "NONE";

    case ATOwner::MODEM:
        return "MODEM";

    case ATOwner::CALL:
        return "CALL";

    case ATOwner::SMS:
        return "SMS";

    case ATOwner::USSD:
        return "USSD";

    case ATOwner::PHONEBOOK:
        return "PHONEBOOK";

    case ATOwner::DEBUG:
        return "DEBUG";
    }

    return "UNKNOWN";
}

bool SIM800::readUserLine(char *buffer, size_t size)
{
    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            if (uiInputPos == 0)
                continue;

            uiInput[uiInputPos] = '\0';

            strncpy(buffer, uiInput, size - 1);
            buffer[size - 1] = '\0';

            uiInputPos = 0;
            uiInput[0] = '\0';

            return true;
        }

        if (uiInputPos < sizeof(uiInput) - 1)
        {
            uiInput[uiInputPos++] = c;
        }
        else
        {
            // Input too long: discard current line.
            uiInputPos = 0;
            uiInput[0] = '\0';

            Serial.println("Input too long.");
        }
    }

    return false;
}

void SIM800::abortOperations()
{
    Serial.println("Aborting all modem operations.");

    callState = CallState::IDLE;
    smsState = SMSState::SMS_IDLE;
    ussdState = USSDState::USSD_IDLE;

    hangupPending = false;

    callerNumber[0] = '\0';

    smsNumber[0] = '\0';
    smsText[0] = '\0';

    smsReadingMessage = false;

    phoneNumber = "";
    message = "";

    uiState = UIState::MAIN_MENU;

    releaseAT();
}

void SIM800::consumeAT()
{
    atCommand.active = false;
    atCommand.finished = false;
    atCommand.result = AT_TIMEOUT;
    atCommand.length = 0;
    atCommand.response[0] = '\0';

    atOwner = ATOwner::NONE;
}

