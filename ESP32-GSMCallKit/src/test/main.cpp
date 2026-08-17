#include "Arduino.h"
#include "PhonebookTest.h"

int main()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("========================================");
    Serial.println(" PHONEBOOK DESKTOP UNIT TEST");
    Serial.println(" No SIM800");
    Serial.println(" No SIM card");
    Serial.println(" No Arduino hardware");
    Serial.println("========================================");

    bool success =
        PhoneBookTest::runAll();

    Serial.println();

    if (success)
    {
        Serial.println(
            "ALL PHONEBOOK TESTS PASSED."
        );

        return 0;
    }

    Serial.println(
        "PHONEBOOK TESTS FAILED."
    );

    return 1;
}
