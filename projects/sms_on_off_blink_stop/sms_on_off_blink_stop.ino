//TODO//
///remove delays is okay during startup, but a cleaner approach is to wait for responses from SIM800.
// add exception handling, eg esp initilization timer for relay time settings make sure old SMS are deleted
HardwareSerial sim800(2);
const int relayPin = 23;
// State changes variables
bool blinkMode = false;
bool relayState = false;
bool waitingForSMSBody = false;
unsigned long previousMillis = 0;
const unsigned long interval = 3000;
const char phoneNumber[] = "XXXXXXX"; //C-style strings (better for memory) Your controller is going to run continuously. 
const char authorizedNumber[] = "XXXXXXX"; //C-style strings (better for memory) Your controller is going to run continuously. 

//A SIM800 relay controller might stay powered for months, so avoiding unnecessary String operations is better.
// Buffer for incoming SMS lines
String incomingLine = "";
bool networkRegistered = false;
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 10000;
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
void sendSMS(String message) {

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
    delay(5000);
}

String sendAT(String cmd) {
    while (sim800.available()) {
        sim800.read();
    }
    Serial.print("TX: ");
    Serial.println(cmd);
    sim800.println(cmd);
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 3000) {
        while (sim800.available()) {
            response += (char)sim800.read();
        }
        if (response.indexOf("OK") >= 0 ||
            response.indexOf("ERROR") >= 0) {
            break;
        }
    }
    Serial.println("RX:");
    Serial.println(response);
    return response;
}
void setRelay(bool state) {
    if (relayState == state)
        return;
    relayState = state;
    digitalWrite(relayPin, state ? HIGH : LOW);
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
  while (true) {
    String r = sendAT("AT");
    if (r.indexOf("OK") >= 0) {
        break;
    }

    delay(1000);
  }
  Serial.println("SIM800 Ready");  // Basic SIM800 setup
  sendAT("AT+CPIN?");
  sendAT("AT+CSQ"); //Check signal quality
  while (true) {
    String r = sendAT("AT+CREG?");
    if (r.indexOf(",1") >= 0 ||
        r.indexOf(",5") >= 0) {
        break;
    }
    Serial.println("Waiting for network...");
    delay(2000);
  }
  Serial.println("Network Registered");  
  networkRegistered = true;
  registrationHandled = true;
  sendAT("AT+CMGF=1");      // CMGF means Cellular Message Format SMS text mode
  sendAT("AT+CMGD=1,4");  //deletes all old sms
  sendAT("AT+CNMI=2,2,0,0,0"); // Forward SMS directly to serial
  Serial.println("System Ready");
}

void loop() {
        // Check registration every 10 seconds
  /* if (millis() - lastCheck >= CHECK_INTERVAL) {
    lastCheck = millis();
    reg = sendAT("AT+CREG?");
    networkRegistered =(reg.indexOf(",1") >= 0 || reg.indexOf(",5") >= 0);
   if (networkRegistered && !registrationHandled) {
      registrationHandled = true;
      Serial.println("Network Registered");
    }
    else if (!networkRegistered && registrationHandled) {
      registrationHandled = false;
      Serial.println("Network Lost");
    }
  }*/
  // -----------------------------
  // Read SIM800 data line by line
  // -----------------------------
  while (sim800.available()) {
    char c = sim800.read(); //Reads data incrementally 
    if (c == '\r') {
      continue;// SIM800 lines usually end with \r\n so we ignore carriage return
    }
    if (c == '\n') {
      incomingLine.trim();
      if (incomingLine.length() == 0) {
        incomingLine = "";
        continue;
      }
        Serial.println("LINE: " + incomingLine);
              // Detect SMS header
        if (incomingLine.startsWith("+CMT:")) {


            int firstQuote = incomingLine.indexOf('"');
            int secondQuote = incomingLine.indexOf('"', firstQuote + 1);

            if (firstQuote >= 0 && secondQuote > firstQuote) {

                smsSender = incomingLine.substring(
                    firstQuote + 1,
                    secondQuote
                );

                Serial.print("SMS sender: ");
                Serial.println(smsSender);
            }
                        waitingForSMSBody = true;
                                    incomingLine = "";
            continue;
        }
        // This is the SMS body
        //incomingLine.toUpperCase();
          //      Serial.println("COMMAND: " + incomingLine);
  //  continue;   // wait for SMS body
        /*if (smsSender != authorizedNumber) {

            Serial.println("Unauthorized SMS ignored");

            incomingLine = "";
            smsSender = "";
            continue;
        }*/
           // SMS body
        if (waitingForSMSBody) {

            waitingForSMSBody = false;

            incomingLine.toUpperCase();

            Serial.println("COMMAND: " + incomingLine);
      // -----------------------------
      // Command handling
      // -----------------------------
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

    if (blinkMode || relayState) {

        blinkMode = false;
setRelay(false);

        printState();
        sendSMS("Blink mode stopped");
    }
}
    incomingLine = "";
    }
    else {
      if (incomingLine.length() < 100) {
    incomingLine += c;
      }
    }
  }

  }
    // -----------------------------
  // Blink mode handler
  // -----------------------------
  if (blinkMode && millis() - previousMillis >= interval) {
      previousMillis = millis();
      
    setRelay(!relayState);   // toggle relay state
      printState();
    }
}
