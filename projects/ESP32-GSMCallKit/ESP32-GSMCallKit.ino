// Phase 7: Add new modules
//// 19. Add SMS class.
//// 7. Add SMS functionality.
//    Support:
//    AT+CMGF
//    AT+CMGS
//    SMS receive notifications.
//     Features:
//        - text mode
//        - send SMS
//        - receive SMS
//        - SMS notifications
//
//  Add PhoneBook class.
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

#include "src/SIM800.h"
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
