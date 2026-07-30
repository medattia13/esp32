#include "SIM800.h"

#define SIM800_RX 16
#define SIM800_TX 17

#define MODEM_BAUD 9600


SIM800::SIM800()
:
modem(2)
{
    state = BOOTING;
    linePos = 0;
    bootStep = 0;
}


void SIM800::begin()
{
    modem.begin(
        MODEM_BAUD,
        SERIAL_8N1,
        SIM800_RX,
        SIM800_TX
    );

    Serial.println();
    Serial.println("SIM800 starting...");

    bootStart = millis();
}


void SIM800::update()
{
    processSerial();

    checkATTimeout();


    switch(state)
    {
        case BOOTING:
            // move your old BOOTING code here
            break;


        case READY:
            // move your old READY code here
            break;


        case DIALING:
            break;


        case IN_CALL:
            break;


        case ERROR:
            break;
    }
}

bool SIM800::sendAT(const char *cmd, uint32_t timeout)
{
    if(atCommand.active)
        return false;

    atCommand.active = true;
}
