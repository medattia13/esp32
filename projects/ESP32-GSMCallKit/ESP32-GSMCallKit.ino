// TODO:
// 5. Move serial input cleanup into separate function.
//    Create cleanInput() helper.
//    Remove duplicated '\r'/'\n' cleanup code.
//    ⏳ NEXT
//
// 6. Add support for storing multiple phone numbers.
//    Create phonebook structure.
//    Add save/delete/list phone number commands.
//    Store numbers in EEPROM/Preferences.
//    ⏳ TODO
//
// 7. Add SMS functionality.
//    Support:
//    AT+CMGF
//    AT+CMGS
//    SMS receive notifications.
//    ⏳ TODO
//
// 8. Improve call handling.
//    Add call timeout.
//    Handle outgoing call failure states.
//    Handle incoming call answer/reject:
//        ATA
//        ATH
//    ⏳ TODO
//
// 9. Improve AT command engine.
//    Add command queue for multiple pending commands.
//    Store command name/type for debugging.
//    Add command-specific timeout values.
//    ⏳ FUTURE
//
// 10. Add modem recovery.
//     Handle SIM800 reset/restart.
//     Recover from ERROR state.
//     Add watchdog protection.
//     ⏳ FUTURE


#include <HardwareSerial.h>
#include <ctype.h>
#include <string.h>
HardwareSerial sim800(2); // UART2 used for SIM800 communication
// ESP32 pins
#define SIM800_RX 16
#define SIM800_TX 17
enum ModemState
{
    BOOTING,
    READY,
    DIALING,
    IN_CALL,
    ERROR
};
enum ATResult
{
    AT_TIMEOUT,
    AT_OK,
    AT_ERROR
};

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MODEM_BAUD = 9600;
const uint32_t MODEM_BOOT_TIME = 5000;
constexpr size_t PHONE_BUF_SIZE = 32;
ModemState modemState = BOOTING;
unsigned long bootStart;
char lineBuffer[128];
size_t linePos = 0;
static uint8_t bootStep = 0;
bool incomingCall = false;
struct ATCommand
{
    bool active = false;
    bool finished = false;
    ATResult result = AT_TIMEOUT;

    char response[256] = {0};
    size_t length = 0;
uint32_t timeout=1000;
    uint32_t startTime = 0;
};

ATCommand atCommand;
bool isEvent(const char *line)
{
    return
        !strcmp(line,"CONNECT") ||
        !strcmp(line,"NO CARRIER") ||
        !strcmp(line,"BUSY") ||
        !strcmp(line,"RING");
}
void processLine(const char *line){
  Serial.print("<< ");
  Serial.println(line);
    // Unsolicited events
  if (strcmp(line, "CONNECT") == 0){
    modemState = IN_CALL;
        atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_OK;
    return;
  }
  else if (strcmp(line, "NO CARRIER") == 0){
    modemState = READY;
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_ERROR;
    return;
  }
  else if (strcmp(line, "BUSY") == 0){
    modemState = READY;
    
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
void processSerial(){
  while (sim800.available()){
    char c = sim800.read();
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
// Validate an international phone number (+ followed by digits).
bool validNumber(const char *num)
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
// Send an AT command
bool sendAT(const char *cmd, uint32_t timeout=1000){
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
  sim800.println(cmd);
  return true;
}

bool atFinished()
{
    return atCommand.finished;
}

ATResult atResult()
{
    return atCommand.result;
}

void checkATTimeout(){
  if (!atCommand.active)
    return;
  if (millis() - atCommand.startTime > atCommand.timeout){
    atCommand.active = false;
    atCommand.finished = true;
    atCommand.result = AT_TIMEOUT;
    Serial.println("AT command timeout");
  }
}
void setup() {
  Serial.begin(SERIAL_BAUD);
  sim800.begin(MODEM_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
    Serial.println();
    Serial.println("SIM800 starting...");

    bootStart = millis();

    modemState = BOOTING;}
void loop() {
  processSerial();          // only UART reader
    // Handle AT command timeout
  checkATTimeout();  
    switch (modemState) {
case BOOTING:
{
    static bool commandSent = false;


    if (!commandSent &&
        !atCommand.active &&
        millis() - bootStart >= MODEM_BOOT_TIME)
    {
        if (bootStep == 0)
        {
            sendAT("AT");
        }
        else
        {
            sendAT("ATE0");
        }

        commandSent = true;
    }


    if (commandSent && atFinished())
    {
        commandSent = false;


        if (atResult() != AT_OK)
        {
            Serial.println("SIM800 failed");
            modemState = ERROR;
            break;
        }


        if (bootStep == 0)
        {
            bootStep = 1;
        }
        else
        {
            Serial.println("SIM800 ready");
            modemState = READY;
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
                        modemState = ERROR;
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
                    sendAT("ATH");
                    Serial.println("Hangup requested");
                }


                else if (validNumber(input))
                {
                    char cmd[48];

                    snprintf(cmd,
                             sizeof(cmd),
                             "ATD%s;",
                             input);


                    Serial.print("Calling ");
                    Serial.println(input);


                    sendAT(cmd,30000);

                    modemState = DIALING;
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
                                            modemState = READY;

                    Serial.println("Ending call");
                }
            }

            break;
        }



        case ERROR:
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
