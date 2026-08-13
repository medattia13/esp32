# ESP32 SIM800 Modem Driver

A lightweight non-blocking SIM800 modem driver for ESP32 using UART AT commands.

The project started as a simple SIM800 sketch and has been refactored into a reusable C++ class architecture with a modem state machine, AT command engine, UART parser, and call handling.

## Features

- ESP32 UART communication with SIM800 module
- Non-blocking modem state machine
- AT command engine with timeout handling
- Line-based UART response parser
- SIM800 boot sequence handling
- Modem initialization sequence
- Voice call support
  - Dial numbers
  - Hang up calls
  - Call state tracking
- Phone number validation
- Support for unsolicited modem responses:
  - `RING`
  - `CONNECT`
  - `NO CARRIER`
  - `BUSY`

## Development Status

- Current status:

  - ART communication
  - AT command handling
  - Response parser
  - Boot sequence
  - Call handling
  - Non-blocking architecture

- Future improvements:

  - SMS support
  - GPRS/data connection
  - Better unsolicited message handling
  - Automatic modem recovery
  - Multiple AT command queue

## Development Notes

This project was developed with the assistance of AI tools for code review, debugging, and architectural discussion. All code was tested and adapted for the target ESP32/SIM800 hardware.

## License
Personal project / educational use.
