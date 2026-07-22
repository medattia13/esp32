HardwareSerial sim800(2);

const int relayPin = 23;

String sms = "";

void setup() {

  Serial.begin(115200);

  sim800.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  delay(1000);

  // SIM800 setup
  sim800.println("AT");
  delay(500);

  sim800.println("AT+CMGF=1");        // SMS text mode
  delay(500);

  sim800.println("AT+CNMI=2,2,0,0,0"); // Send SMS directly to serial
  delay(500);

  Serial.println("READY");
}


void loop() {

  while (sim800.available()) {

    char c = sim800.read();

    Serial.write(c);   // show everything from modem

    if (c == '\n') {

      sms.trim();
      sms.toUpperCase();

      if (sms == "ON") {

        digitalWrite(relayPin, HIGH);

        Serial.println("RELAY ON");
      }


      if (sms == "OFF") {

        digitalWrite(relayPin, LOW);

        Serial.println("RELAY OFF");
      }


      sms = "";
    }
    else {

      if (c != '\r') {
        sms += c;
      }
    }
  }
}
