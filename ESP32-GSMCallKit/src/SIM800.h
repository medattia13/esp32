#ifndef SIM800_H
#define SIM800_H

#include <Arduino.h>
#include <HardwareSerial.h>

#include "ATCommand.h"
enum ModemState { BOOTING, READY, DIALING, IN_CALL, MODEM_ERROR,  WAIT_MODE,
                  USSD_INPUT,
                  DEBUG_MODE,
                  CALL_NUMBER,
                  SMS_NUMBER,
                  SMS_MESSAGE,USSD_PENDING, 
                      PHONEBOOK_MENU
 };
enum SMSState
{
    SMS_IDLE,
    SMS_WAIT_TEXTMODE,
    SMS_WAIT_CHARSET,
    SMS_WAIT_PROMPT,
    SMS_WAIT_RESULT,
    SMS_READING,
    SMS_WAIT_LIST
};
enum USSDState
{
    USSD_IDLE,
    USSD_WAIT_RESULT
};

enum PhonebookState
{
    PB_IDLE,
    PB_WAIT_STORAGE,
    PB_WAIT_READ,
    PB_WAIT_WRITE,
    PB_WAIT_DELETE,
    PB_WAIT_LIST
};

enum PhonebookMenu
{
    PB_MENU_MAIN,
    PB_MENU_ADD_NAME,
    PB_MENU_ADD_NUMBER,
    PB_MENU_ADD_INDEX,
    PB_MENU_DELETE,
    PB_MENU_SEARCH,
    PB_MENU_CALL,
    PB_MENU_VIEW
};


struct PhonebookEntry
{
    uint16_t index;
    char number[32];
    char name[32];
    int type;
};



class SIM800
{
public:
    SIM800();

    void begin();
    void update();

    bool dial(const char *number);
    bool hangup();

    bool sendSMS(const char *number, const char *message);
    bool readSMS();
    bool deleteSMS(uint8_t index);

    bool sendUSSD(String code);

bool selectPhonebook(const char *storage = "SM");
bool readPhonebook(uint8_t index, PhonebookEntry &entry);
bool writePhonebook(uint8_t index,const char *number,const char *name);
bool deletePhonebook(uint8_t index);
//bool findPhonebook(const char *name, PhonebookEntry &entry);
bool listPhonebook();


    ModemState getModemState();
    SMSState getSMSState();
private:
    HardwareSerial modem;

    ATCommand atCommand;

    ModemState state;

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
    
    
    PhonebookState phonebookState = PB_IDLE;
    PhonebookMenu phonebookMenu = PB_MENU_MAIN;
char phonebookSearchName[32];
PhonebookEntry phonebookEntries[20];
PhonebookEntry lastPhonebookEntry;
uint8_t phonebookEntryCount = 0;

uint8_t phonebookIndex = 0;

char phonebookName[32];
char phonebookNumber[32];

void printPhonebookMenu();
void handlePhonebookInput();
void printPhonebookEntry(const PhonebookEntry &entry);
void processPhonebook(const char *line);

    void processSerial();
    void processLine(const char *line);
    void processCall(const char *line);
    void processSMS(const char *line);
    void processUSSD(const char *line);

    bool sendAT(const char *cmd, uint32_t timeout = 1000);

    void checkATTimeout();

    bool validNumber(const char *number);
    bool atFinished();
    ATResult atResult();
    void handleDebugInput();
    void printMenu();
    
    String ucs2ToUtf8(String hex);
    bool isUCS2(String s);
    
    
    
    
};
#endif
