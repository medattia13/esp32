#include <HardwareSerial.h>

HardwareSerial sim800(2);   // UART2

#define SIM800_RX 16
#define SIM800_TX 17
#define RELAY_PIN 26

String desiredOperator = "TUNSIANA";   // Change to your operator

void setup() {
  Serial.begin(115200);

  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  delay(3000);

  sendAT("AT");
  sendAT("ATE0");
}

void loop() {

  String response = sendAT("AT+COPS?");

  Serial.println(response);

  if (response.indexOf(desiredOperator) >= 0) {
    Serial.println("Operator matched!");
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    Serial.println("Operator not matched.");
    digitalWrite(RELAY_PIN, LOW);
  }

  delay(5000);
}

String sendAT(String cmd) {

  while (sim800.available())
    sim800.read();

  sim800.println(cmd);

  String resp = "";
  unsigned long start = millis();

  while (millis() - start < 3000) {
    while (sim800.available()) {
      char c = sim800.read();
      resp += c;
    }
  }

  return resp;
}
