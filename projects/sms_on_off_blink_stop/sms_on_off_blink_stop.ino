//TODO//
///remove delays is okay during startup, but a cleaner approach is to wait for responses from SIM800.
// add exception handling, eg esp initilization timer for relay time settings make sure old SMS are deleted
//A SIM800 relay controller might stay powered for months, so avoiding unnecessary String operations is better.
// Buffer for incoming SMS lines
HardwareSerial sim800(2);
const int relayPin = 23;
bool blinkMode = false;
bool relayState = false;
bool waitingForSMSBody = false;
unsigned long previousMillis = 0;
const unsigned long interval = 3000;
const char phoneNumber[] = "XXX"; //C-style strings (better for memory) Your controller is going to run continuously. 
const char authorizedNumber[] = "XXXXXXX"; //C-style strings (better for memory) Your controller is going to run continuously. 
String incomingLine = "";
bool networkRegistered = false;
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 70000;
unsigned long lastTimeCheck = 0;
String reg = "";
bool registrationHandled = false;
String smsSender = "";
void printState() {
  Serial.println("----- State Changed -----");
  Serial.print("Blink mode: ");
  Serial.println(blinkMode ? "ON" : "OFF");
  Serial.print("Relay: ");
  Serial.println(relayState ? "ON" : "OFF");
  Serial.print("relayPin output: ");
  Serial.println(digitalRead(relayPin));
  Serial.println();
}
bool checkRegistration(bool force = false) {

    unsigned long now = millis();

    if (force || (now - lastCheck >= CHECK_INTERVAL)) {
        lastCheck = now;

        String response = sendAT("AT+CREG?");
        bool registered =
            response.indexOf(",1") != -1 ||
            response.indexOf(",5") != -1;

        if (registered != networkRegistered) {
            networkRegistered = registered;
            Serial.println(
                registered ? "Network Registered" : "Network Lost"
            );
        }
    }

    return networkRegistered;
}
bool waitForPrompt(unsigned long timeout = 5000) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (sim800.available()) {
            char c = sim800.read();
            Serial.write(c);   // Optional: show modem output
            if (c == '>') {
                return true;
            }
        }
    }
    return false;
}
void processSIM800Char(char c) {
  Serial.print('[');
Serial.print((int)c);
Serial.print(']');
Serial.write(c);
Serial.println();
    // Ignore carriage return
    if (c == '\r') {
        return;
    }
    // Build the current line

    if (c != '\n') {
      if (incomingLine.length() < 256) {
        incomingLine += c;  //prevent it from growing forever if the SIM800 sends unexpected data
      }        
      return;
    }
    Serial.println("NEWLINE RECEIVED");

    // End of line reached
    incomingLine.trim();
    if (incomingLine.length() == 0) {
        return;
    }
    Serial.println("LINE: " + incomingLine);
    // If we're waiting for the SMS body, this line is the message
    if (waitingForSMSBody) {
      Serial.println("SMS BODY FOUND");
        waitingForSMSBody = false;
        incomingLine.toUpperCase();
        Serial.println("COMMAND: " + incomingLine);
        // Command handling
      if (incomingLine == "ON") {
        blinkMode = false;
        setRelay(true);
        printState();
        sendSMS("Relay is ON");
      }
      else if (incomingLine == "OFF") {
        blinkMode = false;
        setRelay(false);
        printState();
        sendSMS("Relay OFF");
      }
      else if (incomingLine == "BLINK") {
        if (!blinkMode) {
        blinkMode = true;
        setRelay(true);
        previousMillis = millis();
        printState();
        sendSMS("Blink mode started");
        }
      }
      else if (incomingLine == "STOP") {
        blinkMode = false;
        setRelay(false);
        printState();
        sendSMS("Blink mode stopped");
      }
          else {
      sendSMS("Unknown command");
    }
        incomingLine = "";
        return;
    }
    // Detect SMS header
    if (incomingLine.startsWith("+CMT:")) {
        Serial.println("SMS HEADER FOUND");
        int firstQuote = incomingLine.indexOf('"');
        int secondQuote = incomingLine.indexOf('"', firstQuote + 1);
        if (firstQuote >= 0 && secondQuote > firstQuote) {
            smsSender = incomingLine.substring(firstQuote + 1, secondQuote);
            Serial.print("SMS sender: ");
            Serial.println(smsSender);
        }
        waitingForSMSBody = true;
        incomingLine = "";
        return;
    }
    // Handle other SIM800 responses here  

    incomingLine = "";
    return;
}
void sendSMS(const String &message) {
    if (!networkRegistered)
        return;
    sim800.print("AT+CMGS=\"");
    sim800.print(phoneNumber);
    sim800.println("\"");
    if (!waitForPrompt()) {
        Serial.println("SMS prompt timeout");
        return;
    }
    sim800.print(message);
    sim800.write(26);      // Ctrl+Z
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 10000) {
      while(sim800.available()) {
        response += (char)sim800.read();
      }
      if(response.indexOf("OK") >= 0)
        break;
      delay(1);
    }
}
String sendAT(const String &cmd) {
    String response;
    response.reserve(128);

    // Clear old data from SIM800 buffer
    while (sim800.available()) {
        sim800.read();
    }

    Serial.print(F("TX: "));
    Serial.println(cmd);

    sim800.println(cmd);

    unsigned long start = millis();
    const unsigned long timeout = 3000;

    while (millis() - start < timeout) {

        while (sim800.available()) {
            char c = sim800.read();
            response += c;

            // Only display raw modem response
            Serial.write(c);

            // Stop when command finished
            if (response.indexOf("OK") >= 0 ||
                response.indexOf("ERROR") >= 0) {
                Serial.println();
                Serial.println(F("AT command complete"));
                return response;
            }
        }

        delay(1);
    }

    Serial.println();
    Serial.println(F("AT command timeout"));

    return response;
}
void setRelay(bool state) {
    if (relayState == state)
        return;
    relayState = state;
    digitalWrite(relayPin, state ? HIGH : LOW);
}
void testFakeSMS(String sender, String command) {

    String fakeSMS = 
        "+CMT: \"" + sender + "\",,\"\"\r\n" +
        command + "\r\n";

    Serial.println("---- TEST SMS ----");
    Serial.println(fakeSMS);

    for (int i = 0; i < fakeSMS.length(); i++) {
        processSIM800Char(fakeSMS[i]);
    }

    Serial.println("---- END TEST ----");
}
void setup() {
  //initilization
  blinkMode = false;
  relayState = false;
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("Waiting for SIM800...");
  unsigned long start = millis();
  while (millis() - start < 60000) {
    String r = sendAT("AT");
    if (r.indexOf("OK") >= 0)
        break;
    delay(1000);
  }
  Serial.println("SIM800 Ready");  // Basic SIM800 setup

  sendAT("AT+CPIN?");
  sendAT("AT+CSQ"); //Check signal quality
  start = millis();
while (!checkRegistration(true)) {
    if (millis() - start > 120000) {  // 2 minutes
        Serial.println("Network registration timeout");
        break;
    }

    delay(2000);
}
    if (!networkRegistered) {
    Serial.println("No network. Entering offline mode");
}
else {
    sendAT("ATEO");      // Disable echo
      sendAT("AT+CMEE=1");      // verbose errors

  sendAT("AT+CMGF=1");      // CMGF means Cellular Message Format SMS text mode
  sendAT("AT+CMGD=1,4");  //deletes all old sms
  sendAT("AT+CNMI=2,2,0,0,0"); // Forward SMS directly to serial
  Serial.println("System Ready");
}

  testFakeSMS("+1234567890", "ON");
delay(5000);
testFakeSMS("+1234567890", "OFF");
delay(5000);
testFakeSMS("+1234567890", "BLINK");
delay(10000);
testFakeSMS("+1234567890", "OFF");
}
void loop() {
  // Check registration every 10 seconds
    if (checkRegistration(false)) {
        // Network available
while (sim800.available()) {
    char c = sim800.read();
    Serial.write(c);
    processSIM800Char(c);       
       }
    }
    else {
        // No network
        Serial.println("Waiting for network");
    }
       /*if (smsSender != authorizedNumber) {
            Serial.println("Unauthorized SMS ignored");
            incomingLine = "";
            smsSender = "";
            continue;
        }*/
        // Blink mode handler
  if (blinkMode && millis() - previousMillis >= interval) {
    previousMillis = millis();
    setRelay(!relayState);
    printState();
    }
}
