// TODO:
// 1. Replace blocking delay() during modem boot with millis timer.
//
// 2. Replace manual callActive tracking.
//    Parse SIM800 responses:
//    OK
//    NO CARRIER
//    BUSY
//    RING
//
// 3. Create modem response parser/state machine.
//
// 4. Improve command handling with AT command timeout/error handling.
//
// 5. Move serial input cleanup into separate function.
//
// 6. Add support for storing multiple phone numbers.
//
// 7. Add SMS functionality.

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
char modemResponse[128];
bool modemInitialized = false;
unsigned long bootStart;
bool callActive = false;
void readSIM800Response(char *buffer, size_t size) {
  size_t index = strlen(buffer);
  while (sim800.available()) {
    char c = sim800.read();
    if (index < size - 1) {
      buffer[index++] = c;
      buffer[index] = '\0';
    }
  }
}
//debug only
void printSIM800Response() {
    while (sim800.available()) {
        Serial.write(sim800.read());
    }
}
void callNumber(const char *number) {
    sim800.print("ATD");
    sim800.print(number);
    sim800.println(";");
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
    for (size_t i = 1; num[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)num[i]))
            return false;
    }
    return true;
}
// Send an AT command and print the modem response.
ATResult sendAT(const char *cmd,char *response,size_t responseSize,uint32_t waitTime = 1000){
  Serial.print(">> ");
  Serial.println(cmd);
  sim800.println(cmd);
  size_t index = 0;
  unsigned long start = millis();
  while (millis() - start < waitTime){
    while (sim800.available()){
      char c = sim800.read();
      Serial.write(c);   // optional debug output
      if (index < responseSize - 1){
        response[index++] = c;
        response[index] = '\0';
      }
    }
    // Stop early if we received a final response
        if (strstr(response, "OK")){
            return AT_OK;
        }
        if (strstr(response, "ERROR")){
            return AT_ERROR;
        }
  }
  return AT_TIMEOUT;
}
void setup() {
  Serial.begin(SERIAL_BAUD);
  sim800.begin(MODEM_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  bootStart = millis();
}
void loop() {
  switch (modemState) {
    case BOOTING: {
      if (millis() - bootStart >= MODEM_BOOT_TIME) {
        ATResult result = sendAT("AT", modemResponse, sizeof(response));
        switch (result) {
          case AT_OK:
            Serial.println("SIM800 ready");
            sendAT("ATE0", modemResponse, sizeof(modemResponse));
            modemState = READY;
            break;
          case AT_ERROR:
            Serial.println("SIM800 returned ERROR");
            modemState = ERROR;
            break;
          case AT_TIMEOUT:
            Serial.println("SIM800 timeout");
            modemState = ERROR;
            break;
        }
      }
      break;
    }
    case READY: {
      if (!modemInitialized) {
        ATResult result;
        result = sendAT("AT+CPIN?", modemResponse, sizeof(modemResponse));
        if (result != AT_OK) {
          modemState = ERROR;
          break;
        }
        result = sendAT("AT+CREG?", modemResponse, sizeof(modemResponse));
        if (result != AT_OK) {
          modemState = ERROR;
          break;
        }
        result = sendAT("AT+CSQ", modemResponse, sizeof(modemResponse));
        if (result != AT_OK) {
          modemState = ERROR;
          break;
        }
        modemInitialized = true;
        Serial.println();
        Serial.println("==================================");
        Serial.println("Enter phone number");
        Serial.println("Example: +21612345678");
        Serial.println("Press Enter to call.");
        Serial.println("==================================");
      }
      if (Serial.available()) {
        char input[PHONE_BUF_SIZE];
        size_t len = Serial.readBytesUntil('\n', input, PHONE_BUF_SIZE - 1);
        input[len] = '\0';

        while (len > 0 && (input[len - 1] == '\r' || input[len - 1] == '\n')) {
          input[--len] = '\0';
        }
        if (strcasecmp(input, "H") == 0) {
          if (callActive) {
            sim800.println("ATH");
            callActive = false;
            Serial.println("Call ended.");
          }
        }
        else if (validNumber(input)) {
          Serial.print("Calling ");
          Serial.println(input);
          callNumber(input);
          modemState = DIALING;
        }
        else {
          Serial.println("Invalid phone number.");
        }
      }
      break;
    }
    case DIALING: {
      // TODO: parse modem response
      // CONNECT -> IN_CALL
      // NO CARRIER -> READY
      if (strstr(response, "CONNECT")) {
        modemState = IN_CALL;
        callActive = true;
      }
      break;
    }
    case IN_CALL: {
      if (Serial.available()) {
        char command = Serial.read();
        if (command == 'H' || command == 'h') {
          sim800.println("ATH");
          callActive = false;
          modemState = READY;
          Serial.println("Call ended.");
        }
      }
      break;
    }
    case ERROR: {
      Serial.println("Modem error.");
      while (true) {
      }
      break;
    }
  }
  //printSIM800Response();
  readSIM800Response(modemResponse, sizeof(modemResponse));
}
