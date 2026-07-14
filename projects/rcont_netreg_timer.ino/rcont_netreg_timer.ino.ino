#include <HardwareSerial.h>
HardwareSerial sim800(2);
#define SIM800_RX 16
#define SIM800_TX 17
#define RELAY_PIN 26
unsigned long relayStartTime = 0;
bool relayActive = false;
bool registrationHandled = false;
const unsigned long RELAY_TIME = 60000; // 1 minute
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 10000;
String reg = "";
bool networkRegistered = false;
void setup() {
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Starting SIM800...");
  delay(3000);
  sendAT("AT");
  sendAT("ATE0");
  // Enable network time sync
  Serial.println("Enabling network time...");
  sendAT("AT+CLTS=1");
  // Save setting
  sendAT("AT&W");
sendAT("AT+CFUN=1,1");
Serial.println("Restarting modem...");
Serial.println("Waiting for modem restart...");

while (true) {

    String r = sendAT("AT");

    if (r.indexOf("OK") >= 0) {
        break;
    }

    delay(1000);
}

Serial.println("SIM800 Ready");
  Serial.println("Waiting for GSM registration...");
}
void loop() {

    // Check registration every 10 seconds
    if (millis() - lastCheck >= CHECK_INTERVAL) {

        lastCheck = millis();

        reg = sendAT("AT+CREG?");

        Serial.println("Registration status:");
        Serial.println(reg);

        networkRegistered =
            (reg.indexOf(",1") >= 0 || reg.indexOf(",5") >= 0);

        if (networkRegistered) {

            if (!registrationHandled) {

                Serial.println("================================");
                Serial.println("NETWORK REGISTERED");
                Serial.println("================================");

                showNetworkTime();

                Serial.println("Turning relay ON for 1 minute");

                digitalWrite(RELAY_PIN, HIGH);

                relayStartTime = millis();
                relayActive = true;

                registrationHandled = true;

            }
            else {

                Serial.println("Still registered.");

            }

        }
        else {

            Serial.println("Waiting for network...");
            registrationHandled = false;

        }
    }

    // Relay timer
    if (relayActive && millis() - relayStartTime >= RELAY_TIME) {

        Serial.println("1 minute elapsed");
        Serial.println("Turning relay OFF");

        digitalWrite(RELAY_PIN, LOW);

        relayActive = false;
    }
}

void showNetworkTime() {
  Serial.println("Requesting network time...");
  String time = sendAT("AT+CCLK?");
  Serial.println("Network time response:");
  Serial.println(time);
}

String sendAT(String cmd) {
  while(sim800.available()) {
    sim800.read();

  }
  Serial.print("TX: ");
  Serial.println(cmd);
  sim800.println(cmd);
  String response="";
  unsigned long start=millis();
  while(millis()-start < 3000) {
    while(sim800.available()) {
      char c=sim800.read();
      response += c;
    }
  }
  Serial.println("RX:");
  Serial.println(response);
  return response;
}
