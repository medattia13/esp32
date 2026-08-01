#ifndef SIM800_H
#define SIM800_H

#include <Arduino.h>
#include <HardwareSerial.h>

#include "ATCommand.h"
enum ModemState
{
    BOOTING,
    READY,
    DIALING,
    IN_CALL,
    MODEM_ERROR
};

class SIM800
{

public:

    SIM800();

    void begin();
    void update();

    bool dial(const char *number);
    bool hangup();

    ModemState getState();


private:

    HardwareSerial modem;

    ATCommand atCommand;   // <-- add this here

    ModemState state;

    char lineBuffer[128];
    size_t linePos;

    unsigned long bootStart;
    uint8_t bootStep;

bool incomingCall = false;


    void processSerial();
    void processLine(const char *line);

    bool sendAT(const char *cmd,
                uint32_t timeout = 1000);

    void checkATTimeout();

    bool validNumber(const char *number);
    bool atFinished();
ATResult atResult();

};
#endif
