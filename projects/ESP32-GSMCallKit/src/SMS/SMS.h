#ifndef SMS_H
#define SMS_H

#include <Arduino.h>
#include "SIM800.h"

class SMS
{
public:

    SMS(SIM800 &modem);

    bool begin();

    bool send(const char *number,
              const char *message);

    void update();

private:

    SIM800 &sim800;

    void processSMS(const char *line);
};

#endif
