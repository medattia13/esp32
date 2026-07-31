// TODO: Arduino IDE GSM project refactor
// Phase 6: Test after refactor
//
// 14. Compile in Arduino IDE.
//
// 15. Test modem boot:
//     - AT
//     - ATE0
//
// 16. Test initialization:
//     - CPIN
//     - CREG
//     - CSQ
//
// 17. Test outgoing call:
//     - Dial number
//     - CONNECT
//     - Hangup
//     - NO CARRIER
//
// 18. Fix any regression before adding features.
//
//
// Phase 7: Add new modules
//
// 19. Add PhoneBook class.
//// 6. Add support for storing multiple phone numbers.
//    Create phonebook structure.
//    Add save/delete/list phone number commands.
//    Store numbers in EEPROM/Preferences.
//    ⏳ TODO
//     Files:
//        PhoneBook.h
//        PhoneBook.cpp
//
//     Features:
//        - store numbers
//        - delete numbers
//        - list numbers
//        - ESP32 Preferences storage
//
// 20. Add SMS class.
//// 7. Add SMS functionality.
//    Support:
//    AT+CMGF
//    AT+CMGS
//    SMS receive notifications.
//    ⏳ TODO
//
//     Files:
//        SMS.h
//        SMS.cpp
//
//     Features:
//        - text mode
//        - send SMS
//        - receive SMS
//        - SMS notifications
//
//
// Phase 8: Future improvements
//
// 21. Add AT command queue.
//// 5. Move serial input cleanup into separate function.
//    Create cleanInput() helper.
//    Remove duplicated '\r'/'\n' cleanup code.
//    ⏳ NEXT
// 9. Improve AT command engine.
//    Add command queue for multiple pending commands.
//    Store command name/type for debugging.
//    Add command-specific timeout values.
//    ⏳ FUTURE
// 22. Add modem recovery.
//// 8. Improve call handling.
// 10. Add modem recovery.
//     Handle SIM800 reset/restart.
//     Recover from ERROR state.
//     Add watchdog protection.
//     ⏳ FUTURE

//    Add call timeout.
//    Handle outgoing call failure states.
//    Handle incoming call answer/reject:
//        ATA
//        ATH
//    ⏳ TODO
// 23. Add SIM800 reset handling.
//
// 24. Add watchdog protection.

#include "SIM800.h"
SIM800 modem;

void setup() 
{
      Serial.begin(115200);
  modem.begin();
}
void loop() 
{
  modem.update();
 

}
