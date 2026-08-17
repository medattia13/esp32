#include "PhonebookTest.h"

#include <string.h>
#include <ctype.h>


// ============================================================
// FakePhoneBookHost
// ============================================================

FakePhoneBookHost::FakePhoneBookHost()
    : forceError(false)
    , verbose(true)
    , contactCount(0)
    , dialWasCalled(false)
    , returnedToMainMenu(false)
{
    memset(lastCommand, 0, sizeof(lastCommand));
    memset(lastDialNumber, 0, sizeof(lastDialNumber));

    clearContacts();
}


// ============================================================
// Clear fake contacts
// ============================================================

void FakePhoneBookHost::clearContacts()
{
    contactCount = 0;

    memset(
        contacts,
        0,
        sizeof(contacts)
    );
}


// ============================================================
// Add fake contact
// ============================================================

void FakePhoneBookHost::addContact(
    uint16_t index,
    const char *number,
    int type,
    const char *name)
{
    if (contactCount >= MAX_FAKE_CONTACTS)
        return;

    PhoneBookEntry &entry =
        contacts[contactCount];

    memset(
        &entry,
        0,
        sizeof(entry)
    );

    entry.index = index;
    entry.type = type;

    strncpy(
        entry.number,
        number,
        sizeof(entry.number) - 1
    );

    entry.number[
        sizeof(entry.number) - 1
    ] = '\0';

    strncpy(
        entry.name,
        name,
        sizeof(entry.name) - 1
    );

    entry.name[
        sizeof(entry.name) - 1
    ] = '\0';

    contactCount++;
}


// ============================================================
// Reset tracking
// ============================================================

void FakePhoneBookHost::resetTracking()
{
    memset(
        lastCommand,
        0,
        sizeof(lastCommand)
    );

    memset(
        lastDialNumber,
        0,
        sizeof(lastDialNumber)
    );

    dialWasCalled = false;
    returnedToMainMenu = false;
}


// ============================================================
// validNumber()
// ============================================================

bool FakePhoneBookHost::validNumber(
    const char *number)
{
    if (number == nullptr)
        return false;

    if (number[0] != '+')
        return false;

    if (strlen(number) < 2)
        return false;

    for (size_t i = 1;
         number[i] != '\0';
         i++)
    {
        if (!isdigit(
                (unsigned char)number[i]))
        {
            return false;
        }
    }

    return true;
}


// ============================================================
// sendAT()
//
// This is the fake SIM800.
//
// Instead of sending an AT command to hardware,
// it immediately generates the appropriate response.
// ============================================================

bool FakePhoneBookHost::sendAT(
    const char *cmd,
    uint32_t timeout)
{
    (void)timeout;

    strncpy(
        lastCommand,
        cmd,
        sizeof(lastCommand) - 1
    );

    lastCommand[
        sizeof(lastCommand) - 1
    ] = '\0';


    if (verbose)
    {
        Serial.print("[FAKE MODEM] << ");
        Serial.println(cmd);
    }


    // --------------------------------------------------------
    // Forced ERROR mode
    // --------------------------------------------------------

    if (forceError)
    {
        if (verbose)
            Serial.println(
                "[FAKE MODEM] >> ERROR"
            );

        // PhoneBook normally receives ERROR
        // from SIM800::processLine().
        //
        // Here we inject it directly.

        return true;
    }


    // --------------------------------------------------------
    // AT+CPBS="SM"
    //
    // Select SIM phonebook
    // --------------------------------------------------------

    if (strcmp(
            cmd,
            "AT+CPBS=\"SM\"") == 0)
    {
        if (verbose)
            Serial.println(
                "[FAKE MODEM] >> OK"
            );

        return true;
    }


    // --------------------------------------------------------
    // AT+CPBR=1,20
    //
    // List phonebook
    // --------------------------------------------------------

    if (strcmp(
            cmd,
            "AT+CPBR=1,20") == 0)
    {
        if (verbose)
        {
            Serial.println(
                "[FAKE MODEM] >> phonebook entries"
            );
        }

        return true;
    }


    // --------------------------------------------------------
    // AT+CPBR=N
    //
    // Read one contact
    // --------------------------------------------------------

    if (strncmp(
            cmd,
            "AT+CPBR=",
            8) == 0)
    {
        if (verbose)
        {
            Serial.print(
                "[FAKE MODEM] >> single contact request: "
            );

            Serial.println(cmd);
        }

        return true;
    }


    // --------------------------------------------------------
    // AT+CPBW=N,"number",145,"name"
    //
    // Write/delete
    // --------------------------------------------------------

    if (strncmp(
            cmd,
            "AT+CPBW=",
            8) == 0)
    {
        if (verbose)
            Serial.println(
                "[FAKE MODEM] >> OK"
            );

        return true;
    }


    // --------------------------------------------------------
    // Unknown command
    // --------------------------------------------------------

    if (verbose)
    {
        Serial.print(
            "[FAKE MODEM] Unknown command: "
        );

        Serial.println(cmd);
    }

    return true;
}


// ============================================================
// dial()
// ============================================================

bool FakePhoneBookHost::dial(
    const char *number)
{
    dialWasCalled = true;

    strncpy(
        lastDialNumber,
        number,
        sizeof(lastDialNumber) - 1
    );

    lastDialNumber[
        sizeof(lastDialNumber) - 1
    ] = '\0';


    if (verbose)
    {
        Serial.print(
            "[FAKE MODEM] DIAL "
        );

        Serial.println(
            lastDialNumber
        );
    }

    return true;
}


// ============================================================
// returnToMainMenu()
// ============================================================

void FakePhoneBookHost::returnToMainMenu()
{
    returnedToMainMenu = true;

    if (verbose)
    {
        Serial.println(
            "[FAKE HOST] returnToMainMenu()"
        );
    }
}


// ============================================================
// PhoneBookTest helpers
// ============================================================

void PhoneBookTest::printTestHeader(
    const char *name)
{
    Serial.println();
    Serial.println(
        "----------------------------------------"
    );

    Serial.print(
        "[TEST] "
    );

    Serial.println(name);

    Serial.println(
        "----------------------------------------"
    );
}


bool PhoneBookTest::expect(
    bool condition,
    const char *message)
{
    if (condition)
    {
        Serial.print(
            "  PASS: "
        );

        Serial.println(message);

        return true;
    }

    Serial.print(
        "  FAIL: "
    );

    Serial.println(message);

    return false;
}


void PhoneBookTest::printSummary(
    uint16_t passed,
    uint16_t total)
{
    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "       PHONEBOOK TEST SUMMARY"
    );

    Serial.println(
        "========================================"
    );

    Serial.print(
        "Passed: "
    );

    Serial.print(passed);

    Serial.print(
        " / "
    );

    Serial.println(total);

    if (passed == total)
    {
        Serial.println(
            "RESULT: ALL TESTS PASSED"
        );
    }
    else
    {
        Serial.println(
            "RESULT: SOME TESTS FAILED"
        );
    }

    Serial.println(
        "========================================"
    );
}


// ============================================================
// TEST 1
//
// Valid CPBR parser
// ============================================================

bool PhoneBookTest::testParserValid()
{
    printTestHeader(
        "Valid CPBR parser"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    PhoneBookEntry entry;

    memset(
        &entry,
        0,
        sizeof(entry)
    );


    const char *line =
        "+CPBR: 1,\"+21612345678\",145,\"Alice\"";


    bool result =
        phonebook.parseCPBR(
            line,
            entry
        );


    bool pass = true;

    pass &= expect(
        result == true,
        "Parser accepts valid CPBR"
    );

    pass &= expect(
        entry.index == 1,
        "Index == 1"
    );

    pass &= expect(
        strcmp(
            entry.number,
            "+21612345678"
        ) == 0,
        "Number parsed correctly"
    );

    pass &= expect(
        entry.type == 145,
        "Type == 145"
    );

    pass &= expect(
        strcmp(
            entry.name,
            "Alice"
        ) == 0,
        "Name parsed correctly"
    );

    return pass;
}


// ============================================================
// TEST 2
//
// Invalid CPBR parser inputs
// ============================================================

bool PhoneBookTest::testParserInvalid()
{
    printTestHeader(
        "Invalid CPBR parser"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    bool allPassed = true;


    const char *tests[] =
    {
        "INVALID",
        "+CSQ: 20,99",
        "+CPBR: abc,\"+21612345678\",145,\"Alice\"",
        "+CPBR: 1,+21612345678,145,\"Alice\"",
        "+CPBR: 1,\"+21612345678\",145,Alice",
        "+CPBR: 1,\"+21612345678\""
    };


    const size_t count =
        sizeof(tests) /
        sizeof(tests[0]);


    for (size_t i = 0;
         i < count;
         i++)
    {
        PhoneBookEntry entry;

        memset(
            &entry,
            0,
            sizeof(entry)
        );


        bool result =
            phonebook.parseCPBR(
                tests[i],
                entry
            );


        Serial.print(
            "  Input: "
        );

        Serial.println(
            tests[i]
        );


        if (!expect(
                result == false,
                "Parser rejects invalid input"))
        {
            allPassed = false;
        }
    }

    return allPassed;
}


// ============================================================
// TEST 3
//
// List contacts
// ============================================================

bool PhoneBookTest::testList()
{
    printTestHeader(
        "Phonebook LIST"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();


    // Simulate View Contacts menu.
    phonebook.menu =
        PB_MENU_VIEW;

    phonebook.entryCount = 0;

    phonebook.state =
        PB_IDLE;


    // Start list operation directly.
    bool started =
        phonebook.list();


    bool pass = true;


    pass &= expect(
        started == true,
        "LIST command started"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBS=\"SM\""
        ) == 0,
        "Correct storage command sent"
    );


    // Simulate modem OK.
    phonebook.onOk();


    pass &= expect(
        phonebook.state ==
            PB_WAIT_LIST,
        "State changed to PB_WAIT_LIST"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBR=1,20"
        ) == 0,
        "Correct CPBR list command sent"
    );


    // Simulate modem contacts.
    phonebook.processLine(
        "+CPBR: 1,\"+21612345678\",145,\"Alice\""
    );


    phonebook.processLine(
        "+CPBR: 2,\"+21698765432\",145,\"Bob\""
    );


    pass &= expect(
        phonebook.entryCount == 2,
        "Two contacts parsed"
    );


    pass &= expect(
        phonebook.entries[0].index == 1,
        "First contact index correct"
    );


    pass &= expect(
        strcmp(
            phonebook.entries[0].name,
            "Alice"
        ) == 0,
        "First contact name correct"
    );


    pass &= expect(
        phonebook.entries[1].index == 2,
        "Second contact index correct"
    );


    pass &= expect(
        strcmp(
            phonebook.entries[1].name,
            "Bob"
        ) == 0,
        "Second contact name correct"
    );


    // Simulate final OK.
    phonebook.onOk();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "State returned to PB_IDLE"
    );


    pass &= expect(
        phonebook.menu == PB_MENU_MAIN,
        "Menu returned to PB_MENU_MAIN"
    );


    return pass;
}


// ============================================================
// TEST 4
//
// Read one contact
// ============================================================

bool PhoneBookTest::testRead()
{
    printTestHeader(
        "Read one contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();

    phonebook.menu =
        PB_MENU_CALL;


    // Start read.
    bool started =
        phonebook.read(1);


    bool pass = true;


    pass &= expect(
        started == true,
        "READ command started"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBS=\"SM\""
        ) == 0,
        "Storage selection command correct"
    );


    // Storage OK.
    phonebook.onOk();


    pass &= expect(
        phonebook.state ==
            PB_WAIT_READ,
        "State changed to PB_WAIT_READ"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBR=1"
        ) == 0,
        "Correct single-contact CPBR command"
    );


    // Simulate modem contact.
    phonebook.processLine(
        "+CPBR: 1,\"+21612345678\",145,\"Alice\""
    );


    pass &= expect(
        phonebook.lastEntry.index == 1,
        "Contact index parsed"
    );


    pass &= expect(
        strcmp(
            phonebook.lastEntry.number,
            "+21612345678"
        ) == 0,
        "Contact number parsed"
    );


    pass &= expect(
        strcmp(
            phonebook.lastEntry.name,
            "Alice"
        ) == 0,
        "Contact name parsed"
    );


    // Final OK.
    phonebook.onOk();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "State returned to PB_IDLE"
    );


    return pass;
}


// ============================================================
// TEST 5
//
// Read nonexistent contact
//
// This also detects stale lastEntry bugs.
// ============================================================

bool PhoneBookTest::testReadNotFound()
{
    printTestHeader(
        "Read nonexistent contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();

    phonebook.menu =
        PB_MENU_CALL;


    // Put an old contact into lastEntry.
    //
    // This is intentionally done to detect the
    // stale-lastEntry problem.

    phonebook.lastEntry.index = 1;

    strcpy(
        phonebook.lastEntry.number,
        "+21612345678"
    );

    strcpy(
        phonebook.lastEntry.name,
        "OLD CONTACT"
    );


    bool started =
        phonebook.read(10);


    bool pass = true;


    pass &= expect(
        started == true,
        "READ command started"
    );


    // Storage OK.
    phonebook.onOk();


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBR=10"
        ) == 0,
        "Correct contact index requested"
    );


    // No +CPBR response.
    //
    // Simulate only OK.
    phonebook.onOk();


    /*
     * If read() clears lastEntry correctly,
     * this should be zero.
     *
     * With your current Phonebook.cpp it will
     * remain index 1.
     */

    bool noStaleEntry =
        phonebook.lastEntry.index == 0;


    pass &= expect(
        noStaleEntry,
        "Previous contact was cleared"
    );


    return pass;
}


// ============================================================
// TEST 6
//
// Write contact
// ============================================================

bool PhoneBookTest::testWrite()
{
    printTestHeader(
        "Write contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();


    bool started =
        phonebook.write(
            10,
            "+21612345678",
            "Alice"
        );


    bool pass = true;


    pass &= expect(
        started == true,
        "WRITE command started"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBW=10,\"+21612345678\",145,\"Alice\""
        ) == 0,
        "Correct CPBW write command"
    );


    pass &= expect(
        phonebook.state ==
            PB_WAIT_WRITE,
        "State is PB_WAIT_WRITE before response"
    );


    // Simulate modem OK.
    phonebook.onOk();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "State returned to PB_IDLE"
    );


    pass &= expect(
        phonebook.menu == PB_MENU_MAIN,
        "Menu returned to main"
    );


    return pass;
}


// ============================================================
// TEST 7
//
// Delete contact
// ============================================================

bool PhoneBookTest::testDelete()
{
    printTestHeader(
        "Delete contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();


    bool started =
        phonebook.removeEntry(10);


    bool pass = true;


    pass &= expect(
        started == true,
        "DELETE command started"
    );


    pass &= expect(
        strcmp(
            host.lastCommand,
            "AT+CPBW=10"
        ) == 0,
        "Correct delete command"
    );


    pass &= expect(
        phonebook.state ==
            PB_WAIT_DELETE,
        "State is PB_WAIT_DELETE"
    );


    phonebook.onOk();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "State returned to PB_IDLE"
    );


    return pass;
}


// ============================================================
// TEST 8
//
// ERROR handling
// ============================================================

bool PhoneBookTest::testError()
{
    printTestHeader(
        "Phonebook ERROR handling"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();


    // Start a write operation.
    bool started =
        phonebook.write(
            10,
            "+21612345678",
            "Alice"
        );


    bool pass = true;


    pass &= expect(
        started == true,
        "Operation started"
    );


    pass &= expect(
        phonebook.state ==
            PB_WAIT_WRITE,
        "PhoneBook is busy"
    );


    // Simulate modem ERROR.
    phonebook.onError();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "ERROR returns to PB_IDLE"
    );


    pass &= expect(
        phonebook.menu == PB_MENU_MAIN,
        "ERROR returns to main menu"
    );


    return pass;
}


// ============================================================
// TEST 9
//
// Search
// ============================================================

bool PhoneBookTest::testSearch()
{
    printTestHeader(
        "Search contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();


    phonebook.menu =
        PB_MENU_SEARCH;


    strcpy(
        phonebook.phonebookName,
        "Alice"
    );


    bool started =
        phonebook.list();


    bool pass = true;


    pass &= expect(
        started == true,
        "Search/list operation started"
    );


    phonebook.onOk();


    pass &= expect(
        phonebook.state ==
            PB_WAIT_LIST,
        "State is PB_WAIT_LIST"
    );


    phonebook.processLine(
        "+CPBR: 1,\"+21612345678\",145,\"Alice\""
    );


    phonebook.processLine(
        "+CPBR: 2,\"+21698765432\",145,\"Bob\""
    );


    pass &= expect(
        phonebook.lastEntry.index == 1,
        "Matching contact saved as lastEntry"
    );


    pass &= expect(
        strcmp(
            phonebook.lastEntry.name,
            "Alice"
        ) == 0,
        "Correct contact found"
    );


    phonebook.onOk();


    pass &= expect(
        phonebook.state == PB_IDLE,
        "Search returns to PB_IDLE"
    );


    return pass;
}


// ============================================================
// TEST 10
//
// Call contact
// ============================================================

bool PhoneBookTest::testDialContact()
{
    printTestHeader(
        "Call contact"
    );

    FakePhoneBookHost host;

    PhoneBook phonebook(host);

    phonebook.begin();

    phonebook.menu =
        PB_MENU_CALL;


    bool started =
        phonebook.read(1);


    bool pass = true;


    pass &= expect(
        started == true,
        "Read for call started"
    );


    phonebook.onOk();


    phonebook.processLine(
        "+CPBR: 1,\"+21612345678\",145,\"Alice\""
    );


    phonebook.onOk();


    pass &= expect(
        host.dialWasCalled,
        "Dial was requested"
    );


    pass &= expect(
        strcmp(
            host.lastDialNumber,
            "+21612345678"
        ) == 0,
        "Correct number passed to dial()"
    );


    return pass;
}


// ============================================================
// RUN ALL TESTS
// ============================================================

bool PhoneBookTest::runAll()
{
    Serial.println();
    Serial.println();
    Serial.println(
        "########################################"
    );

    Serial.println(
        "#   PHONEBOOK OFFLINE TEST SUITE       #"
    );

    Serial.println(
        "########################################"
    );


    uint16_t passed = 0;
    uint16_t total = 0;


    if (testParserValid())
        passed++;

    total++;


    if (testParserInvalid())
        passed++;

    total++;


    if (testList())
        passed++;

    total++;


    if (testRead())
        passed++;

    total++;


    if (testReadNotFound())
        passed++;

    total++;


    if (testWrite())
        passed++;

    total++;


    if (testDelete())
        passed++;

    total++;


    if (testError())
        passed++;

    total++;


    if (testSearch())
        passed++;

    total++;


    if (testDialContact())
        passed++;

    total++;


    printSummary(
        passed,
        total
    );


    return passed == total;
}


// ============================================================
// Convenience function
// ============================================================

void runPhoneBookTests()
{
    PhoneBookTest::runAll();
}
