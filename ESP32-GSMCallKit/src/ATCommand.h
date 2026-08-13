#ifndef ATCOMMAND_H
#define ATCOMMAND_H

#include <Arduino.h>

enum ATResult { AT_TIMEOUT, AT_OK, AT_ERROR };

struct ATCommand
{
    bool active = false;
    bool finished = false;

    ATResult result = AT_TIMEOUT;

    char response[256] = {0};
    size_t length = 0;

    uint32_t timeout = 1000;
    uint32_t startTime = 0;
};

#endif
