#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include "Arduino.h"
#include <stdint.h>

struct PhoneBookEntry
{
    uint16_t index;
    int type;

    char number[32];
    char name[64];
};

enum PhonebookState
{
    PB_IDLE,
    PB_WAIT_STORAGE,
    PB_WAIT_LIST,
    PB_WAIT_READ,
    PB_WAIT_WRITE,
    PB_WAIT_DELETE
};

enum PhonebookMenu
{
    PB_MENU_MAIN,

    PB_MENU_ADD_NAME,
    PB_MENU_ADD_NUMBER,
    PB_MENU_ADD_INDEX,

    PB_MENU_VIEW,
    PB_MENU_DELETE,
    PB_MENU_SEARCH,
    PB_MENU_CALL
};


// ------------------------------------------------------------
// Interface used by PhoneBook to communicate with SIM800
// ------------------------------------------------------------

class IPhoneBookHost
{
public:
    virtual bool sendAT(const char *cmd, uint32_t timeout) = 0;

    virtual bool dial(const char *number) = 0;

    virtual bool validNumber(const char *number) = 0;

    virtual void returnToMainMenu() = 0;

    virtual ~IPhoneBookHost() {}

};


// ------------------------------------------------------------
// PhoneBook
// ------------------------------------------------------------

class PhoneBook
{
public:

    explicit PhoneBook(IPhoneBookHost &host);

    void begin();

    void open();

    void update();

    // Called by SIM800 when a +CPBR line arrives
    void processLine(const char *line);

    // Called by SIM800 when AT command returns OK
    void onOk();

    // Called by SIM800 when AT command returns ERROR
    void onError();

    bool isBusy() const;

    PhonebookState getState() const;
    PhonebookMenu getMenu() const;
    

private:
    // Allow the offline test class to access private
    // PhoneBook functions without making them public.
    friend class PhoneBookTest;
    
    IPhoneBookHost &host;

    PhonebookState state;
    PhonebookMenu menu;

    PhoneBookEntry lastEntry;

    PhoneBookEntry entries[20];
    uint8_t entryCount;

    uint8_t phonebookIndex;

    char phonebookName[64];
    char phonebookNumber[32];

    void printMenu();
    void printEntry(const PhoneBookEntry &entry);

    void handleInput();

    bool selectStorage(const char *storage);

    bool write(
        uint8_t index,
        const char *number,
        const char *name
    );

    bool removeEntry(uint8_t index);

    bool read(uint8_t index);

    bool list();

    void handleEntry(const PhoneBookEntry &entry);

    bool parseCPBR(
        const char *line,
        PhoneBookEntry &entry
    );

    void resetToMainMenu();
};

#endif
