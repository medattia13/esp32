#include <HardwareSerial.h>

HardwareSerial sim800(2);

// ESP32 pins
#define SIM800_RX 16
#define SIM800_TX 17

String phoneNumber = "";
String message = "";
String ussdCode = "";

enum Mode {
  WAIT_MODE,
  SMS_NUMBER,
  SMS_MESSAGE,
  USSD_INPUT,
  AT_MODE
};

Mode mode = WAIT_MODE;

void manualAT(String cmd) {
  sim800.println(cmd);
}

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

void setupSMSReceive() {
  sendAT("AT+CMGF=1");              // SMS text mode
  sendAT("AT+CSCS=\"GSM\"");         // Normal character set
  // New SMS indication directly to UART
  sendAT("AT+CNMI=2,2,0,0,0");
  Serial.println("SMS receiving enabled.");
}

void readSMS() {
  Serial.println("Reading SMS...");
  sim800.println("AT+CMGL=\"ALL\"");
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (sim800.available()) {
      Serial.write(sim800.read());
    }
  }
}

void sendSMS(String number, String text) {
  Serial.println("Sending SMS...");

  sim800.println("AT+CMGF=1");
  delay(500);

  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  delay(500);

  sim800.print(text);
  delay(200);

  sim800.write(26);   // Ctrl+Z
}

void sendUSSD(String code) {
  Serial.println("Cancelling previous USSD session...");

  // Cancel any active USSD session
  sim800.println("AT+CUSD=2");
  delay(1000);

  // Clear any pending responses
  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  Serial.print("Sending USSD: ");
  Serial.println(code);

  sim800.print("AT+CUSD=1,\"");
  sim800.print(code);
  sim800.println("\"");

  // Wait for the network response
  unsigned long start = millis();
  while (millis() - start < 10000) {   // Wait up to 10 seconds
    while (sim800.available()) {
      Serial.write(sim800.read());
    }
  }
}

void printMenu() {
  Serial.println();
  Serial.println("========== MENU ==========");
  Serial.println("SMS      - Send SMS");
  Serial.println("USSD     - Send USSD code");
  Serial.println("READSMS  - Read stored SMS");
  Serial.println("E        - Manual AT mode");
  Serial.println("==========================");
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
  sendAT("AT+CMGF=1");
  setupSMSReceive();
  printMenu();
}

void loop() {
  // Always display SIM800 responses
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
  if (!Serial.available())
    return;
  String input = Serial.readStringUntil('\n');
  input.trim();
  // Manual AT mode
  if (mode == AT_MODE) {
    if (input.equalsIgnoreCase("EXIT")) {
      mode = WAIT_MODE;
      Serial.println();
      Serial.println("Leaving AT mode.");
      printMenu();
      return;
    }
    manualAT(input);
    return;
  }
  switch (mode) {
    case WAIT_MODE:
      input.toUpperCase();
      if (input == "SMS") {
        Serial.println("Phone number:");
        mode = SMS_NUMBER;
      }
      else if (input == "USSD") {
        Serial.println("Enter USSD code (example: *100#):");
        mode = USSD_INPUT;
      }
      else if (input == "READSMS") {
        readSMS();
        printMenu();
      } 
      else if (input == "E") {
        mode = AT_MODE;
        Serial.println();
        Serial.println("===== MANUAL AT MODE =====");
        Serial.println("Type AT commands directly.");
        Serial.println("Example: AT+CSQ");
        Serial.println("Type EXIT to return.");
        Serial.println("==========================");
      }
      else {
        printMenu();
      }
      break;
    case SMS_NUMBER:
      phoneNumber = input;
      Serial.println("Message:");
      mode = SMS_MESSAGE;
      break;
    case SMS_MESSAGE:
      message = input;
      sendSMS(phoneNumber, message);
      Serial.println("SMS sent.");
      phoneNumber = "";
      message = "";
      mode = WAIT_MODE;
      printMenu();
      break;
    case USSD_INPUT:
      ussdCode = input;
      sendUSSD(ussdCode);
      mode = WAIT_MODE;
      printMenu();
      break;
    case AT_MODE:
      break;
  }
}
