#include "SMS.h"

SMS::SMS(SIM800 &modem) :
    sim800(modem)
{
}


bool SMS::begin()
{
    sim800.sendAT("AT+CMGF=1");
    return true;
}


bool SMS::send(const char *number,
               const char *message)
{
    char cmd[64];

    snprintf(cmd,
             sizeof(cmd),
             "AT+CMGS=\"%s\"",
             number);

    sim800.sendAT(cmd);

    // wait for '>' prompt
    // send message
    // send Ctrl+Z

    return true;
}


void SMS::update()
{
    // handle +CMTI
}
