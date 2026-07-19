#include <HardwareSerial.h>

HardwareSerial sim800(2);

// ESP32 pins
#define SIM800_RX 16
#define SIM800_TX 17

String phoneNumber = "";

void sendAT(String cmd, int waitTime = 1000) {
  Serial.print(">> ");
  Serial.println(cmd);

  sim800.println(cmd);

  unsigned long start = millis();
  while (millis() - start < waitTime) {
    while (sim800.available()) {
      Serial.write(sim800.read());
    }
  }
}

void setup() {
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  delay(5000);

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

  // Read phone number
  if (Serial.available()) {
    phoneNumber = Serial.readStringUntil('\n');
    phoneNumber.trim();

    if (phoneNumber.length() > 0) {
      Serial.print("Calling ");
      Serial.println(phoneNumber);

      sim800.print("ATD");
      sim800.print(phoneNumber);
      sim800.println(";");

      delay(1000);

      while (sim800.available()) {
        Serial.write(sim800.read());
      }

      Serial.println();
      Serial.println("Type another number to call again.");
      Serial.println("Type H to hang up.");
    }
  }

  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  // Hang up
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "H") {
      sim800.println("ATH");
      Serial.println("Call ended.");
    }
  }
}
