#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include "Phonebook.h"
#include "ATCommand.h"
#include "ATOwner.h"



// ============================================================
// STATE MACHINES
// ============================================================


enum class ModemState
{
    BOOTING,
    INITIALIZING,
    READY,
    MODEM_ERROR,
    MODEM_RECOVERING
};

enum class CallState
{
    IDLE,
    DIALING,
    IN_CALL,
    INCOMING_CALL
};

enum class SMSState
{
    SMS_IDLE,
    SMS_WAIT_TEXTMODE,
    SMS_WAIT_CHARSET,
    SMS_WAIT_PROMPT,
    SMS_WAIT_RESULT,
    SMS_READING,
    SMS_WAIT_LIST
};

enum class USSDState
{
    USSD_IDLE,
    USSD_WAIT_RESULT
};

enum class UIState
{
    MAIN_MENU,
    CALL_NUMBER,
    SMS_NUMBER,
    SMS_MESSAGE,
    USSD_INPUT,
    USSD_WAIT_RESULT,
    PHONEBOOK,
    DEBUG_MODE
};


// ============================================================
// SIM800
// ============================================================

class SIM800 : public IPhoneBookHost
{
public:

    SIM800();

    void begin();
    void update();

    // --------------------------------------------------------
    // IPhoneBookHost
    // --------------------------------------------------------

    bool sendAT(const char *cmd, uint32_t timeout, ATOwner owner) override;

    bool dial(
        const char *number
        ) override;

    bool validNumber(
        const char *number
        ) override;

    void returnToMainMenu() override;


    // --------------------------------------------------------
    // Call
    // --------------------------------------------------------

    bool hangup();


    // --------------------------------------------------------
    // SMS
    // --------------------------------------------------------

    bool sendSMS(
        const char *number,
        const char *message
        );

    bool readSMS();

    bool deleteSMS(
        uint8_t index
        );


    // --------------------------------------------------------
    // USSD
    // --------------------------------------------------------

    bool sendUSSD(
        const String &code
        );


    // --------------------------------------------------------
    // State
    // --------------------------------------------------------

    ModemState getModemState() const;

    CallState getCallState() const;

    SMSState getSMSState() const;

    USSDState getUSSDState() const;

    UIState getUIState() const;


private:

    // ========================================================
    // HARDWARE / COMPONENTS
    // ========================================================

    HardwareSerial modem;

    ATCommand atCommand;

    PhoneBook phonebook;


    // ========================================================
    // STATE
    // ========================================================

    ModemState modemState;

    CallState callState;

    SMSState smsState;

    USSDState ussdState;

    UIState uiState;


    // ========================================================
    // MODEM LIFECYCLE
    // ========================================================

    uint8_t modemInitStep;

    uint8_t recoveryStep;

    uint32_t recoveryStart;

    uint8_t recoveryAttempts;


    // ========================================================
    // SERIAL INPUT
    // ========================================================

    char lineBuffer[128];

    size_t linePos;


    // ========================================================
    // USER INPUT
    // ========================================================

    String phoneNumber;

    String message;


    // ========================================================
    // SMS
    // ========================================================

    char smsNumber[32];

    char smsText[161];

    // SMS reading
    uint8_t smsReadIndex;

    char smsReadStatus[16];

    char smsReadSender[32];

    char smsReadDate[32];

    bool smsReadingMessage;


    // ========================================================
    // CALL
    // ========================================================

    char callerNumber[32];

    uint32_t callStartTime;


    // ========================================================
    // MODEM BOOT
    // ========================================================

    uint32_t bootStart;

    uint8_t bootStep;


    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr uint32_t DIAL_TIMEOUT = 30000;

    static constexpr uint32_t MODEM_BOOT_TIME = 5000;

    static constexpr uint8_t MAX_RECOVERY_ATTEMPTS = 3;


    // ========================================================
    // MAIN STATE MACHINES
    // ========================================================

    void updateModem();

    void updateCall();

    void updateUI();


    // ========================================================
    // MODEM STATE HANDLERS
    // ========================================================

    void updateModemBoot();

    void updateModemInitialization();

    void startModemRecovery();

    void updateModemRecovery();


    // ========================================================
    // CALL STATE HANDLERS
    // ========================================================

    void updateDialingCall();

    void updateIncomingCall();

    void updateActiveCall();


    // ========================================================
    // UI STATE HANDLERS
    // ========================================================

    void updateUIMainMenu();

    void updateUICallNumber();

    void updateUISMSNumber();

    void updateUISMSMessage();

    void updateUIUSSDInput();


    // ========================================================
    // MODEM RESPONSE DISPATCH
    // ========================================================

    void processSerial();

    void processLine(
        const char *line
        );


    // ========================================================
    // RESPONSE HANDLERS
    // ========================================================

    bool handleCallLine(
        const char *line
        );

    bool handleSMSLine(
        const char *line
        );

    bool handleUSSDLine(
        const char *line
        );

    bool handlePhonebookLine(
        const char *line
        );

    bool handleATResponse(
        const char *line
        );


    // ========================================================
    // CALL RESPONSE HELPERS
    // ========================================================

    void handleCallerID(
        const char *line
        );


    // ========================================================
    // SMS HELPERS
    // ========================================================

    bool parseSMSHeader(
        const char *line
        );


    void printSMSHeader();


    // ========================================================
    // AT COMMAND
    // ========================================================

    void checkATTimeout();
    void finishAT(ATResult result);
void releaseAT();
    bool atFinished();

    ATResult atResult();
ATOwner atOwner;


    // ========================================================
    // DEBUG / UI
    // ========================================================

    void handleDebugInput();

    void printMenu();

    void cleanInput(char *input );
    const char* atOwnerName();
    
};
