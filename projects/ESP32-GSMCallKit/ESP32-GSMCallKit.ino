// TODO: 
//Replace String with fixed-size char[] buffers to reduce heap allocations.
// callActive logic has one limitation A future version should parse modem responses:
//Replace delay() during modem boot (optional
#include <HardwareSerial.h>
HardwareSerial sim800(2); // UART2 used for SIM800 communication
// ESP32 pins
#define SIM800_RX 16
#define SIM800_TX 17
const uint32_t SERIAL_BAUD = 115200;
const uint32_t MODEM_BAUD = 9600;
const uint32_t MODEM_BOOT_TIME = 5000;
bool callActive = false;
void printSIM800Response() {
    while (sim800.available()) {
        Serial.write(sim800.read());
    }
}
void callNumber(const String &number) {
    sim800.print("ATD");
    sim800.print(number);
    sim800.println(";");
}
// Validate an international phone number (+ followed by digits).
bool validNumber(const String &num) {
    if (!num.startsWith("+"))
        return false;
    if (num.length() < 2)
        return false;
    for (size_t i = 1; i < num.length(); i++) {
        if (!isDigit(num[i]))
            return false;
    }
    return true;
}
// Send an AT command and print the modem response.
void sendAT(const String &cmd, uint32_t waitTime = 1000) {
    Serial.print(">> ");
    Serial.println(cmd);
    sim800.println(cmd);
    unsigned long start = millis();
    while (millis() - start < waitTime) {
        printSIM800Response();
    }
}
void setup() {
  Serial.begin(SERIAL_BAUD);
  sim800.begin(MODEM_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(MODEM_BOOT_TIME);
  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CPIN?");
  sendAT("AT+CREG?");
  sendAT("AT+CSQ");
  Serial.println();
  Serial.println("==================================");
  Serial.println("Enter phone number (international format)");
  Serial.println("Example: +21612345678");
  Serial.println("Press Enter to call.");
  Serial.println("==================================");
}
void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("H")) {
        if (callActive) {
            sim800.println("ATH");
            callActive = false;
            Serial.println("Call ended.");
        }
    } else if (validNumber(input)) {
      if (callActive) {
        Serial.println("A call is already in progress.");
        return;
      }
      Serial.print("Calling ");
      Serial.println(input);
      callNumber(input);
      callActive = true;
    } else {
        Serial.println("Invalid phone number.");
    }
  }
printSIM800Response();
}
