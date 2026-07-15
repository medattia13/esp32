#include <HardwareSerial.h>

HardwareSerial sim800(2);

#define SIM800_RX 16
#define SIM800_TX 17

void sendAT(String cmd, int wait = 1000) {

  Serial.print("\nTX: ");
  Serial.println(cmd);

  sim800.println(cmd);

  unsigned long start = millis();

  while (millis() - start < wait) {

    while (sim800.available()) {

      char c = sim800.read();
      Serial.write(c);

    }
  }
}


void setup() {

  Serial.begin(115200);

  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  delay(1000);

  Serial.println("SIM800 SMS MONITOR");

  sendAT("AT");
  sendAT("ATE0");             // disable echo
  sendAT("AT+CPIN?");
  sendAT("AT+CSQ");
  sendAT("AT+CREG?");
  
  // SMS text mode
  sendAT("AT+CMGF=1");

  // Push received SMS directly to serial
  sendAT("AT+CNMI=2,2,0,0,0");

  Serial.println("\nREADY - SEND SMS TO SIM");
}


void loop() {

  while (sim800.available()) {

    char c = sim800.read();

    Serial.write(c);   // show everything from modem

  }


  while (Serial.available()) {

    char c = Serial.read();

    sim800.write(c);   // allow typing AT commands manually

  }

}
