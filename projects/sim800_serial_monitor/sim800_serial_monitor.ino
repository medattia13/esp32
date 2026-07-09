HardwareSerial sim800(2);

const int ledPin = 2;   // Change to your LED pin

void setup() {
  Serial.begin(115200);
  sim800.begin(115200, SERIAL_8N1, 16, 17); // RX=16 TX=17

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  Serial.println("Ready");
  Serial.println("Enter ON, OFF, or any AT command.");
}

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

  // Print SIM800 responses
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}
