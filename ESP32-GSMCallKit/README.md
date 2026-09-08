# ESP32 SIM800 Modem Driver

A lightweight, non-blocking SIM800 modem driver for ESP32 using UART AT commands.

The project uses a reusable C++ architecture with separate state machines for modem initialization, calls, SMS, USSD, and phonebook operations.

## Features

- ESP32 UART communication with SIM800
- Non-blocking state-machine architecture
- AT command handling with timeouts and ownership
- UART response parsing
- Modem boot, initialization, and recovery
- Voice calls
  - Dial, answer, and hang up
  - Incoming call detection
  - Call state tracking
- SMS
  - Send, read, and delete
- USSD support
- Phonebook
  - Add, view, search, delete, call, and SMS contacts
- Phone number validation
- Handling of unsolicited responses such as `RING`, `CONNECT`, `BUSY`, and `NO CARRIER`

## Development Status

### Implemented

- AT command engine
- UART parser
- Modem state machine
- Call handling
- SMS handling
- USSD handling
- Phonebook handling
- Modem recovery
- Non-blocking architecture

### Future Improvements

- GPRS/data support
- More robust unsolicited response handling
- AT command queue
- Improved SMS/phonebook encoding support

## Development Notes

The project was developed with assistance from AI tools for code review, debugging, and architectural discussion. Code was tested and adapted for the target ESP32/SIM800 hardware.

## License

Personal project / educational use.

