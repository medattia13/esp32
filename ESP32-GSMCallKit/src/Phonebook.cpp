//TODO: implement the search function

#include "Phonebook.h"
#include "Encoding.h"

PhoneBook::PhoneBook(IPhoneBookHost &host)
    : host(host)
    , state(PB_IDLE)
    , menu(PB_MENU_MAIN)
    , entryCount(0)
    , phonebookIndex(0)
{
    memset(&lastEntry, 0, sizeof(lastEntry));
    memset(entries, 0, sizeof(entries));

    phonebookName[0] = '\0';
    phonebookNumber[0] = '\0';
}


void PhoneBook::begin()
{
    state = PB_IDLE;
    menu = PB_MENU_MAIN;

    entryCount = 0;

    phonebookName[0] = '\0';
    phonebookNumber[0] = '\0';
}


void PhoneBook::open()
{
    menu = PB_MENU_MAIN;
    state = PB_IDLE;

    entryCount = 0;

    printMenu();
}


void PhoneBook::update()
{
    if (isBusy())
        return;

    handleInput();
}



bool PhoneBook::isBusy() const
{
    return state != PB_IDLE;
}


PhonebookState PhoneBook::getState() const
{
    return state;
}


PhonebookMenu PhoneBook::getMenu() const
{
    return menu;
}


// ============================================================
// MENU
// ============================================================

void PhoneBook::printMenu()
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


void PhoneBook::printEntry(const PhoneBookEntry &entry)
{
    Serial.print("[");
    Serial.print(entry.index);
    Serial.print("] ");

    Serial.print(entry.name);

    Serial.print(" : ");

    Serial.println(entry.number);
}


// ============================================================
// INPUT
// ============================================================

void PhoneBook::handleInput()
{
    if (!Serial.available())
        return;

    String input = Serial.readStringUntil('\n');
    input.trim();

    switch (menu)
    {
        // ----------------------------------------------------
        // MAIN MENU
        // ----------------------------------------------------

        case PB_MENU_MAIN:

            if (input == "1")
            {
                Serial.println();
                Serial.println("--- ADD CONTACT ---");
                Serial.println("Enter name:");

                menu = PB_MENU_ADD_NAME;
            }

            else if (input == "2")
            {
                Serial.println();
                Serial.println("--- CONTACTS ---");

                menu = PB_MENU_VIEW;
                entryCount = 0;

                if (!list())
                {
                    Serial.println("Unable to read phonebook.");
                    resetToMainMenu();
                }
            }

            else if (input == "3")
            {
                Serial.println();
                Serial.println("--- DELETE CONTACT ---");
                Serial.println("Enter contact index:");

                menu = PB_MENU_DELETE;
            }

            else if (input == "4")
            {
                Serial.println();
                Serial.println("--- SEARCH CONTACT ---");
                Serial.println("Enter name:");

                menu = PB_MENU_SEARCH;
            }

            else if (input == "5")
            {
                Serial.println();
                Serial.println("--- CALL CONTACT ---");
                Serial.println("Enter contact index:");

                menu = PB_MENU_CALL;
            }

            else if (input == "6")
            {
                host.returnToMainMenu();
            }

            else
            {
                Serial.println("Invalid option.");
                printMenu();
            }

            break;


        // ----------------------------------------------------
        // ADD - NAME
        // ----------------------------------------------------

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

            menu = PB_MENU_ADD_NUMBER;

            break;


        // ----------------------------------------------------
        // ADD - NUMBER
        // ----------------------------------------------------

        case PB_MENU_ADD_NUMBER:

            input.toCharArray(
                phonebookNumber,
                sizeof(phonebookNumber)
            );

            if (!host.validNumber(phonebookNumber))
            {
                Serial.println("Invalid phone number.");
                Serial.println("Example: +21612345678");
                break;
            }

            Serial.println("Enter SIM contact index:");
            Serial.println("Example: 1");

            menu = PB_MENU_ADD_INDEX;

            break;


        // ----------------------------------------------------
        // ADD - INDEX
        // ----------------------------------------------------

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

            if (write(
                    phonebookIndex,
                    phonebookNumber,
                    phonebookName))
            {
                Serial.println("Saving contact...");
            }
            else
            {
                Serial.println("Unable to save contact.");
                resetToMainMenu();
            }

            break;
        }


        // ----------------------------------------------------
        // DELETE
        // ----------------------------------------------------

        case PB_MENU_DELETE:
        {
            int index = input.toInt();

            if (index < 1 || index > 250)
            {
                Serial.println("Invalid index.");
                break;
            }

            phonebookIndex = index;

            if (removeEntry(phonebookIndex))
            {
                Serial.println("Deleting contact...");
            }
            else
            {
                Serial.println("Unable to delete contact.");
                resetToMainMenu();
            }

            break;
        }


        // ----------------------------------------------------
        // SEARCH
        // ----------------------------------------------------

        case PB_MENU_SEARCH:

            input.toCharArray(
                phonebookName,
                sizeof(phonebookName)
            );

            Serial.println("Searching...");

            /*
             * Search is performed while CPBR entries arrive.
             */

            if (!list())
            {
                Serial.println("Unable to search phonebook.");
                resetToMainMenu();
            }

            break;


        // ----------------------------------------------------
        // CALL
        // ----------------------------------------------------

        case PB_MENU_CALL:
        {
            int index = input.toInt();

            if (index < 1 || index > 250)
            {
                Serial.println("Invalid index.");
                break;
            }

            phonebookIndex = index;

            PhoneBookEntry entry;

            if (read(phonebookIndex))
            {
                Serial.println("Reading contact...");
            }
            else
            {
                Serial.println("Unable to read contact.");
                resetToMainMenu();
            }

            break;
        }


        default:

            resetToMainMenu();

            break;
    }
}


// ============================================================
// STORAGE
// ============================================================

bool PhoneBook::selectStorage(const char *storage)
{
    char cmd[32];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CPBS=\"%s\"",
        storage
    );

    return host.sendAT(cmd, 3000);
}


// ============================================================
// WRITE
// ============================================================

bool PhoneBook::write(
    uint8_t index,
    const char *number,
    const char *name)
{
    if (!host.validNumber(number))
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

    state = PB_WAIT_WRITE;

    if (!host.sendAT(cmd, 5000))
    {
        state = PB_IDLE;
        return false;
    }

    return true;
}


// ============================================================
// DELETE
// ============================================================

bool PhoneBook::removeEntry(uint8_t index)
{
    char cmd[32];

    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CPBW=%u",
        index
    );

    state = PB_WAIT_DELETE;

    if (!host.sendAT(cmd, 5000))
    {
        state = PB_IDLE;
        return false;
    }

    return true;
}


// ============================================================
// READ ONE CONTACT
// ============================================================

bool PhoneBook::read(uint8_t index)
{
    phonebookIndex = index;

    // Important: invalidate previous result
    memset(&lastEntry, 0, sizeof(lastEntry));

    state = PB_WAIT_STORAGE;

    if (!selectStorage("SM"))
    {
        state = PB_IDLE;
        return false;
    }

    return true;
}




// ============================================================
// LIST CONTACTS
// ============================================================

bool PhoneBook::list()
{
    entryCount = 0;

    state = PB_WAIT_STORAGE;

    return selectStorage("SM");
}


// ============================================================
// OK RESPONSE FROM MODEM
// ============================================================

void PhoneBook::onOk()
{
    switch (state)
    {
        case PB_WAIT_STORAGE:
        {
            if (menu == PB_MENU_VIEW ||
                menu == PB_MENU_SEARCH)
            {
                state = PB_WAIT_LIST;

                host.sendAT(
                    "AT+CPBR=1,20",
                    5000
                );
            }
            else
            {
                state = PB_WAIT_READ;

                char cmd[32];

                snprintf(
                    cmd,
                    sizeof(cmd),
                    "AT+CPBR=%u",
                    phonebookIndex
                );

                host.sendAT(cmd, 5000);
            }

            return;
        }


        case PB_WAIT_WRITE:

            Serial.println();
            Serial.println("Contact saved successfully.");

            resetToMainMenu();

            return;


        case PB_WAIT_DELETE:

            Serial.println();
            Serial.println("Contact deleted successfully.");

            resetToMainMenu();

            return;


        case PB_WAIT_LIST:

            Serial.println();
            Serial.println("Finished reading phonebook.");

            if (entryCount == 0)
            {
                Serial.println("No contacts found.");
            }

            resetToMainMenu();

            return;


        case PB_WAIT_READ:

            /*
             * The +CPBR line should already have populated
             * lastEntry.
             */

            if (lastEntry.index != 0)
            {
                printEntry(lastEntry);

                if (menu == PB_MENU_CALL)
                {
                    Serial.print("Calling ");
                    Serial.println(lastEntry.number);

                    host.dial(lastEntry.number);

                    menu = PB_MENU_MAIN;
                    state = PB_IDLE;

                    return;
                }
            }
            else
            {
                Serial.println("Contact not found.");
            }

            resetToMainMenu();

            return;


        default:
            break;
    }
}


// ============================================================
// ERROR RESPONSE
// ============================================================

void PhoneBook::onError()
{
    if (state == PB_IDLE)
        return;

    Serial.println("Phonebook operation failed.");

    resetToMainMenu();
}


// ============================================================
// CPBR PARSER
// ============================================================

bool PhoneBook::parseCPBR(
    const char *line,
    PhoneBookEntry &entry)
{
    memset(&entry, 0, sizeof(entry));

    if (strncmp(line, "+CPBR:", 6) != 0)
        return false;

    const char *p = line + 6;

    while (*p == ' ')
        p++;

    char *endPtr;

    long index = strtol(p, &endPtr, 10);

    if (endPtr == p)
    {
        Serial.println("Invalid phonebook index.");
        return false;
    }

    entry.index = (uint16_t)index;

    p = endPtr;

    while (*p == ' ' || *p == ',')
        p++;

    // --------------------------------------------------------
    // NUMBER
    // --------------------------------------------------------

    if (*p != '"')
    {
        Serial.println("Invalid phonebook number.");
        return false;
    }

    p++;

    const char *numberStart = p;
    const char *numberEnd = strchr(numberStart, '"');

    if (!numberEnd)
    {
        Serial.println("Invalid phonebook number.");
        return false;
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
    // TYPE
    // --------------------------------------------------------

    while (*p == ' ' || *p == ',')
        p++;

    long type = strtol(p, &endPtr, 10);

    if (endPtr == p)
    {
        Serial.println("Invalid phonebook type.");
        return false;
    }

    entry.type = (int)type;

    p = endPtr;

    while (*p == ' ' || *p == ',')
        p++;

    // --------------------------------------------------------
    // NAME
    // --------------------------------------------------------

    if (*p != '"')
    {
        Serial.println("Invalid phonebook name.");
        return false;
    }

    p++;

    const char *nameStart = p;
    const char *nameEnd = strchr(nameStart, '"');

    if (!nameEnd)
    {
        Serial.println("Invalid phonebook name.");
        return false;
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

    return true;
}


// ============================================================
// PROCESS CPBR ENTRY
// ============================================================

void PhoneBook::processLine(const char *line)
{
    PhoneBookEntry entry;

    if (!parseCPBR(line, entry))
        return;

    lastEntry = entry;

    // --------------------------------------------------------
    // VIEW
    // --------------------------------------------------------

    if (menu == PB_MENU_VIEW)
    {
        if (entryCount < 20)
        {
            entries[entryCount++] = entry;
        }

        printEntry(entry);

        return;
    }


    // --------------------------------------------------------
    // SEARCH
    // --------------------------------------------------------

    if (menu == PB_MENU_SEARCH)
    {
        /*
         * Later you can replace this with a case-insensitive
         * UTF-8/UCS2-aware comparison.
         */

        if (strcasecmp(
                entry.name,
                phonebookName) == 0)
        {
            Serial.println();
            Serial.println("Contact found:");

            printEntry(entry);

            lastEntry = entry;
        }

        return;
    }


    // --------------------------------------------------------
    // CALL
    // --------------------------------------------------------

    if (menu == PB_MENU_CALL)
    {
        lastEntry = entry;

        return;
    }
}


// ============================================================
// RESET
// ============================================================

void PhoneBook::resetToMainMenu()
{
    state = PB_IDLE;
    menu = PB_MENU_MAIN;

    phonebookName[0] = '\0';
    phonebookNumber[0] = '\0';

    printMenu();
}
