const int ledPin = 2;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  Serial.println("Type ON or OFF");
}

void loop() {
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
      Serial.println("Unknown command");
    }
  }
}
