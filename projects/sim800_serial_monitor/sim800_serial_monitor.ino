#include <HardwareSerial.h>
HardwareSerial sim800(2);
const int ledPin = 2;   // Change to your LED pin
unsigned long lastCheck = 0;
void setup() {
  Serial.begin(115200);
  delay(3000);
  sim800.begin(9600, SERIAL_8N1, 16, 17); // RX=16 TX=17

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  Serial.println("Ready");
  Serial.println("Enter ON, OFF, or any AT command.");
}
/*void loop() {
  // Auto send AT every 3 sec
  if (millis() - lastCheck > 3000) {
    Serial.println("Sending: AT");
    sim800.println("AT");
    lastCheck = millis();
  }

  // Read from SIM800 → Serial Monitor
  while (sim800.available()) {
    char c = sim800.read();
    Serial.write(c);
  }

  // Send from Serial Monitor → SIM800
  while (Serial.available()) {
    char c = Serial.read();
    sim800.write(c);
  }
}
*/
void loop() {
  // Read commands from Serial Monitor
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "ON") {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED ON");
    }
    else if (cmd == "OFF") {
      digitalWrite(ledPin, LOW);
      Serial.println("LED OFF");
    }
    else {
      // Forward any other command to SIM800
      sim800.println(cmd);
    }
  }
    delay(3000);

  // Print SIM800 responses
  while (sim800.available()) {
    Serial.write(sim800.read());
//    updateSerial();
  }
  
}
