#ifndef PHONEBOOK_TEST_H
#define PHONEBOOK_TEST_H

#include "Arduino.h"
#include <stdint.h>

#include "Phonebook.h"


// ============================================================
// Fake host
//
// This replaces SIM800 for offline testing.
//
// PhoneBook talks to this object instead of the real modem.
// ============================================================

class FakePhoneBookHost : public IPhoneBookHost
{
public:

    FakePhoneBookHost();

    // --------------------------------------------------------
    // IPhoneBookHost implementation
    // --------------------------------------------------------

    bool sendAT(
        const char *cmd,
        uint32_t timeout
    ) override;

    bool dial(
        const char *number
    ) override;

    bool validNumber(
        const char *number
    ) override;

    void returnToMainMenu() override;


    // --------------------------------------------------------
    // Test configuration
    // --------------------------------------------------------

    // When true, every AT command returns ERROR.
    bool forceError;

    // When true, the fake modem prints all activity.
    bool verbose;


    // --------------------------------------------------------
    // Fake modem contacts
    // --------------------------------------------------------

    static constexpr uint8_t MAX_FAKE_CONTACTS = 5;

    PhoneBookEntry contacts[MAX_FAKE_CONTACTS];

    uint8_t contactCount;


    // --------------------------------------------------------
    // Information captured from PhoneBook
    // --------------------------------------------------------

    char lastCommand[128];

    char lastDialNumber[32];

    bool dialWasCalled;

    bool returnedToMainMenu;


    // --------------------------------------------------------
    // Helpers
    // --------------------------------------------------------

    void clearContacts();

    void addContact(
        uint16_t index,
        const char *number,
        int type,
        const char *name
    );

    void resetTracking();
};


// ============================================================
// PhoneBook offline test suite
// ============================================================

class PhoneBookTest
{
public:

    // Run the complete test suite.
    static bool runAll();


private:

    static bool testParserValid();

    static bool testParserInvalid();

    static bool testList();

    static bool testRead();

    static bool testReadNotFound();

    static bool testWrite();

    static bool testDelete();

    static bool testError();

    static bool testSearch();

    static bool testDialContact();

    static void printTestHeader(
        const char *name
    );

    static bool expect(
        bool condition,
        const char *message
    );

    static void printSummary(
        uint16_t passed,
        uint16_t total
    );
};


// Convenience function.
//
// You can simply call:
//
//     runPhoneBookTests();
//
// from setup().
//
// ============================================================

void runPhoneBookTests();

#endif
