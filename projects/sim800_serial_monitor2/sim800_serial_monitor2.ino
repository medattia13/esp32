// Include necessary headers
#include <HardwareSerial.h>

// Hardware configuration
#define MODEM_RX_PIN         16  // ESP32 pin connected to SIM800L TX, must connect
#define MODEM_TX_PIN         17  // ESP32 pin connected to SIM800L RX, must connect
#define MODEM_RST_PIN        5   // Reset pin, set to -1 to ignore
#define MODEM_PWRKEY_PIN     4   // Power key pin, must connect
#define MODEM_PWR_EXT_PIN    23  // External power control, set to -1 to ignore
#define MODEM_BAUD_RATE      9600

// Create a hardware serial for the modem
HardwareSerial HSerial1(1);
//HardwareSerial sim800(2);

// Create SIM800L instance
SIM800L sim800(HSerial1);

const int ledPin = 2;   // Change to your LED pin
 
void setup() {
  // Initialize serial for debugging
  Serial.begin(MODEM_BAUD_RATE);
  Serial2.begin(MODEM_BAUD_RATE);
  delay(3000);
  // Initialize SIM800L module with your pin configuration
  sim800.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN, MODEM_PWRKEY_PIN, MODEM_RST_PIN, MODEM_PWR_EXT_PIN); // RX=16 TX=17

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  Serial.println("Ready");
  Serial.println("Enter ON, OFF, or any AT command.");
}

void loop() {
    // Run the SIM800L state machine
  sim800.loop();
  
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
   // updateSerial();
  }
  
}
