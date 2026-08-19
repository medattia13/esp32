#ifndef SIM800_H
#define SIM800_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Phonebook.h"

#include "ATCommand.h"

enum class ModemState {
    BOOTING,
    READY,
    MODEM_ERROR,
    MODEM_RECOVERING
};
enum class CallState {
    IDLE,
    DIALING,
    IN_CALL,
    INCOMING_CALL
};

enum class SMSState {
    SMS_IDLE,
    SMS_WAIT_TEXTMODE,
    SMS_WAIT_CHARSET,
    SMS_WAIT_PROMPT,
    SMS_WAIT_RESULT,
    SMS_READING,
    SMS_WAIT_LIST
};

enum class USSDState {
    USSD_IDLE,
    USSD_WAIT_RESULT
};
enum class UIState {
    WAIT_MODE,
    CALL_NUMBER,
    SMS_NUMBER,
    SMS_MESSAGE,
    USSD_PENDING,
    DEBUG_MODE,
    PHONEBOOK_MENU
};

class SIM800 : public IPhoneBookHost
{
public:
    SIM800();

    void begin();
    void update();

    bool sendAT(const char *cmd, uint32_t timeout = 1000) override;
    
    bool dial(const char *number) override;
    bool validNumber(const char *number) override;
    void returnToMainMenu() override;
    
    bool hangup();

    bool sendSMS(const char *number, const char *message);
    bool readSMS();
    bool deleteSMS(uint8_t index);

    bool sendUSSD(String code);

    ModemState getModemState();
    SMSState getSMSState();
    
    
private:
    HardwareSerial modem;

    ATCommand atCommand;

    ModemState state; //to be removed relplaced

    ModemState modemState;
    CallState callState;
    SMSState smsState;
    USSDState ussdState;
    UIState uiState;

    char lineBuffer[128];
    size_t linePos;
    SMSState smsState = SMS_IDLE;
    String phoneNumber;

    char smsNumber[32];
    char smsText[161];
    String message;
    unsigned long bootStart;
    uint8_t bootStep;
    bool debugMode = false;
    bool incomingCall = false;
    char callerNumber[32];
   
    uint32_t callStartTime = 0;

static constexpr uint32_t CALL_TIMEOUT = 30000;
 
    PhoneBook phonebook;

    void processSerial();
    void processLine(const char *line);
    void processCall(const char *line);
    void processSMS(const char *line);
    void processUSSD(const char *line);

    void checkATTimeout();

    bool atFinished();
    ATResult atResult();
    void handleDebugInput();
    void printMenu();
   
    void cleanInput(char *input);

};
#endif
