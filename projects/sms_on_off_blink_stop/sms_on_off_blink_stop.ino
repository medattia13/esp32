HardwareSerial sim800(2);

const int relayPin = 23;

bool blinkMode = false;
bool relayState = false;

unsigned long previousMillis = 0;
const unsigned long interval = 3000;

// Buffer for incoming SMS lines
String incomingLine = "";
void checkSIM800Status() {
  sim800.println("AT");
  delay(500);

  sim800.println("AT+CPIN?");
  delay(500);

  sim800.println("AT+CSQ");
  delay(500);

  sim800.println("AT+CREG?");
  delay(500);
}
void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  Serial.begin(115200);

  sim800.begin(9600, SERIAL_8N1, 16, 17);

  delay(3000);

  // Basic SIM800 setup
  sim800.println("AT");
  delay(500);

  sim800.println("AT+CMGF=1");      // SMS text mode
  delay(500);

  sim800.println("AT+CNMI=2,2,0,0,0"); // Forward SMS directly to serial
  delay(500);

  Serial.println("System Ready");
}
/*
void loop() {

  if (sim800.available()) {

    String msg = sim800.readString();
    msg.toUpperCase();

    Serial.println(msg);

    if (msg.indexOf("ON") >= 0) {
      blinkMode = false;
      digitalWrite(relayPin, HIGH);
    }

    else if (msg.indexOf("OFF") >= 0) {
      blinkMode = false;
      digitalWrite(relayPin, LOW);
    }

    else if (msg.indexOf("BLINK") >= 0) {
      blinkMode = true;
      relayState = true;
      digitalWrite(relayPin, HIGH);
      previousMillis = millis();
    }

    else if (msg.indexOf("STOP") >= 0) {
      blinkMode = false;
      digitalWrite(relayPin, LOW);
    }
  }

  if (blinkMode) {
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();
      relayState = !relayState;
      digitalWrite(relayPin, relayState);
    }
  }
}*/
void loop() {

  // -----------------------------
  // Read SIM800 data line by line
  // -----------------------------
  while (sim800.available()) {
    char c = sim800.read();

    if (c == '\n') {
      incomingLine.trim();
      incomingLine.toUpperCase();
      Serial.println("RX: " + incomingLine);

      // -----------------------------
      // Command handling
      // -----------------------------
      if (incomingLine == "ON") {
        blinkMode = false;
        digitalWrite(relayPin, HIGH);
      }

      else if (incomingLine == "OFF") {
        blinkMode = false;
        digitalWrite(relayPin, LOW);
      }

      else if (incomingLine == "BLINK") {
        blinkMode = true;
        relayState = true;
        digitalWrite(relayPin, HIGH);
        previousMillis = millis();
      }

      else if (incomingLine == "STOP") {
        blinkMode = false;
        digitalWrite(relayPin, LOW);
      }

      incomingLine = "";
    }
    else {
      incomingLine += c;
    }
  }

  // -----------------------------
  // Blink mode handler
  // -----------------------------
  if (blinkMode) {
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();
      relayState = !relayState;
      digitalWrite(relayPin, relayState);
    }
  }
  Serial.print("blinkMode: ");
  Serial.println(blinkMode);
  Serial.print("relayState: ");
  Serial.println(relayState);
  Serial.print("relayPin output: ");
  Serial.println(digitalRead(relayPin));
}
