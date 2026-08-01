#ifndef SIM800_H
#define SIM800_H

#include <Arduino.h>
#include <HardwareSerial.h>

#include "ATCommand.h"
enum ModemState { BOOTING, READY, DIALING, IN_CALL, MODEM_ERROR };

class SIM800
{
public:
    SIM800();

    void begin();
    void update();

    bool dial(const char *number);
    bool hangup();

    bool sendSMS(const char *number, const char *message);
    bool readSMS();
    bool deleteSMS(uint8_t index);

    bool sendUSSD(const char *code);
    ModemState getState();

private:
    HardwareSerial modem;

    ATCommand atCommand;

    ModemState state;

    char lineBuffer[128];
    size_t linePos;

    unsigned long bootStart;
    uint8_t bootStep;
    bool debugMode = false;
    bool incomingCall = false;
    char callerNumber[32];
    void processSerial();
    void processLine(const char *line);
    void processCall(const char *line);

    void processSMS(const char *line);
    void processUSSD(const char *line);

    bool sendAT(const char *cmd, uint32_t timeout = 1000);

    void checkATTimeout();

    bool validNumber(const char *number);
    bool atFinished();
    ATResult atResult();
    void handleDebugInput();

};
#endif
