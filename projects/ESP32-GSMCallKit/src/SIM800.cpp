#include "SIM800.h"
#include <ctype.h>
#include <string.h>
#define SIM800_RX 16
#define SIM800_TX 17

constexpr uint32_t MODEM_BAUD = 9600;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t MODEM_BOOT_TIME = 5000;
constexpr size_t PHONE_BUF_SIZE = 32;

SIM800::SIM800()
:
modem(2),
state(BOOTING),
linePos(0),
bootStep(0),
bootStart(0)
{
    lineBuffer[0] = '\0';
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
            {
                static bool commandSent = false;

 if (!commandSent &&
        !atCommand.active &&
        millis() - bootStart >= MODEM_BOOT_TIME)
    {
        if (bootStep == 0)
        {
if(sendAT("AT"))
{
    commandSent = true;
}        }
        else
        {
if(sendAT("ATE0"))
{
    commandSent = true;
}        }

        commandSent = true;
    }


    if (commandSent && atFinished())
    {
        commandSent = false;


        if (atResult() != AT_OK)
        {
            Serial.println("SIM800 failed");
            state = MODEM_ERROR;
            break;
        }


        if (bootStep == 0)
        {
            bootStep = 1;
        }
        else
        {
            Serial.println("SIM800 ready");
            state = READY;
        }


        atCommand.finished = false;
    }


    break;
}
        case READY:
           {
            static bool initialized = false;
            static uint8_t initStep = 0;


            // modem initialization sequence
            if (!initialized)
            {
                if (!atCommand.active && !atCommand.finished)
                {
                    switch(initStep)
                    {
                        case 0:
                            sendAT("AT+CPIN?", 3000);
                            break;

                        case 1:
                            sendAT("AT+CREG?", 3000);
                            break;

                        case 2:
                            sendAT("AT+CSQ", 2000);
                            break;

                        case 3:
                            initialized = true;

                            Serial.println();
                                    Serial.println("Modem initialized");
                            Serial.println("==============================");
                            Serial.println("Enter phone number");
                            Serial.println("Example: +21612345678");
                            Serial.println("H = hangup");
                            Serial.println("==============================");

                            break;
                    }
                }


                if (atFinished())
                {
                    if (atResult() != AT_OK)
                    {
                        state = MODEM_ERROR;
                                break;
                    }

                    atCommand.finished = false;
                        initStep++;
                }

                break;
            }



            // User input
            if (Serial.available())
            {
                char input[PHONE_BUF_SIZE];

                size_t len =
                    Serial.readBytesUntil('\n',
                                          input,
                                          PHONE_BUF_SIZE - 1);

                input[len] = '\0';


                while (len > 0 &&
                      (input[len-1]=='\r' ||
                       input[len-1]=='\n'))
                {
                    input[--len]='\0';
                }


                if (strcasecmp(input,"H")==0)
                {
    hangup();
                }


                else if (validNumber(input))
                {
                        dial(input);
                }


                else
                {
                    Serial.println("Invalid phone number");
                }
            }


            break;
        } 


          


        case DIALING:
              {
            /*
              No blocking here.

              The parser changes state:

              CONNECT
                  |
                  v
              IN_CALL

              NO CARRIER
              BUSY
                  |
                  v
              READY
            */


            break;
        }


        case IN_CALL:
                    {
            if (Serial.available())
            {
                char c = Serial.read();

                if (c=='h' || c=='H')
                {
                    sendAT("ATH");

                    Serial.println("Ending call");
                }
            }

            break;
        }


        case MODEM_ERROR:
                    {
            Serial.println("Modem error");

            while(true)
            {
                // stop
            }

            break;
        }
    }
}

bool SIM800::dial(const char *number)
{
    if (state != READY)
        return false;

    if (!validNumber(number))
        return false;


    char cmd[48];

    snprintf(cmd,
             sizeof(cmd),
             "ATD%s;",
             number);


    Serial.print("Calling ");
    Serial.println(number);


    if(sendAT(cmd,30000))
    {
        state = DIALING;
        return true;
    }


    return false;
}
bool SIM800::hangup()
{
    if(sendAT("ATH",5000))
    {
        Serial.println("Hangup requested");
        return true;
    }

    return false;
}

void SIM800::processLine(const char *line)
{// TODO: Replace strcpy() in processLine() response handling with memcpy()
//       to avoid copying the null terminator twice and make length handling cleaner.

  Serial.print("<< ");
  Serial.println(line);
    // Unsolicited events
  if (strcmp(line, "CONNECT") == 0){
    state = IN_CALL;
        atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_OK;
    return;
  }
  else if (strcmp(line, "NO CARRIER") == 0){
    state = READY;
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_ERROR;
    return;
  }
  else if (strcmp(line, "BUSY") == 0){
    state = READY;
    
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_ERROR;
    return;
  }
  else if (strcmp(line, "RING") == 0){
        incomingCall = true;
    Serial.println("Incoming call");
    return;
  }  
  // AT command final responses
  if (strcmp(line, "OK") == 0){
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_OK;
    return;
  }
  if (strcmp(line, "ERROR") == 0){
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_ERROR;
    return;
  }
    // Normal command response
  if (atCommand.active){
    size_t len = strlen(line);
    if (atCommand.length + len + 2 < sizeof(atCommand.response)){
      strcpy(&atCommand.response[atCommand.length], line);
      atCommand.length += len;
      atCommand.response[atCommand.length++] = '\n';
      atCommand.response[atCommand.length] = '\0';
    }
  }

}
void SIM800::processSerial()
{
  while (modem.available()){
    char c = modem.read();
    if (c == '\r')
      continue;
    if (c == '\n'){
      if (linePos) {
        lineBuffer[linePos] = 0;
        processLine(lineBuffer);
        linePos = 0;
      }
    }
    else if (linePos < sizeof(lineBuffer) - 1){
      lineBuffer[linePos++] = c;
    }
  }
}

// Send an AT command
bool SIM800::sendAT(const char *cmd, uint32_t timeout){
  if (atCommand.active){
        Serial.println("AT command busy");
            return false; // Another command is running
  }
  atCommand.active = true;
  atCommand.finished = false;
  atCommand.result = AT_TIMEOUT;
  atCommand.length = 0;
  atCommand.response[0] = '\0';
  atCommand.startTime = millis();
      atCommand.timeout = timeout;
  Serial.print(">> ");
  Serial.println(cmd);
  modem.println(cmd);
  return true;
}



void SIM800::checkATTimeout(){
  if (!atCommand.active)
    return;
  if (millis() - atCommand.startTime > atCommand.timeout){
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_TIMEOUT;
    Serial.println("AT command timeout");
  }
}


// Validate an international phone number (+ followed by digits).
bool SIM800::validNumber(const char *num)
{
    if (num == nullptr)
        return false;
    if (num[0] != '+')
        return false;
    if (strlen(num) < 2)
        return false;
    for (size_t i = 1; num[i] != '\0'; i++){
      if (!isdigit((unsigned char)num[i]))
        return false;
    }
    return true;
}

bool SIM800::atFinished()
{
    return atCommand.finished;
}


ATResult SIM800::atResult()
{
    return atCommand.result;
}
