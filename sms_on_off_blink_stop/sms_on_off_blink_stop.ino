//TODO//
/*For a long-running SIM800 controller, the general rule is:

    sendAT() owns the serial port temporarily.
    SMS parser owns the serial port during incoming data.
    Never have both active at the same time.
    */
// character analysis to understand the issue when reading from sil800
///remove delays is okay during startup, but a cleaner approach is to wait for responses from SIM800.
// add exception handling, eg esp initilization timer for relay time settings make sure old SMS are deleted
//A SIM800 relay controller might stay powered for months, so avoiding unnecessary String operations is better.
// Buffer for incoming SMS lines
HardwareSerial sim800(2);
#define RX_BUFFER_SIZE 256

const int relayPin = 23;
bool blinkMode = false;
bool relayState = false;
bool waitingForSMSBody = false;
unsigned long previousMillis = 0;
const unsigned long interval = 3000;
const char phoneNumber[] = "XXX"; //C-style strings (better for memory) Your controller is going to run continuously. 
const char authorizedNumber[] = "XXXXXXX"; //C-style strings (better for memory) Your controller is going to run continuously. 
//String incomingLine = "";
char incomingBuffer[RX_BUFFER_SIZE];
size_t incomingIndex = 0;
bool networkRegistered = false;
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 10000;
unsigned long lastTimeCheck = 0;
String reg = "";
bool registrationHandled = false;
String smsSender = "";
bool modemReady = false;
bool modemBusy = false;
//pending SMS response flag command queue
String pendingSMS = "";
bool sendSMSPending = false;
bool startupPhase = true;
void printHexByte(uint8_t b){
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
}
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
String sendAT(const String &cmd, unsigned long timeout = 3000){
        modemBusy = true;
    // Only clear buffer before AT commands
/*if (!networkRegistered && !startupPhase)
{
    modemBusy = false;
    return "";
}*/
    String response;
    response.reserve(128);
if (startupPhase)
{
    while (sim800.available())
        sim800.read();
}

    Serial.print(F("TX: "));
    Serial.println(cmd);
    sim800.println(cmd);
    
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
        while (sim800.available())
        {
            char c = sim800.read();
            response += c;
            // Only display raw modem response
            Serial.write(c);
            // Stop when command finished
            if (response.indexOf("OK") != -1 ||
                response.indexOf("ERROR") != -1)
            {
                  modemBusy = false;
                Serial.println();
                Serial.println(F("AT command complete"));
                    modemBusy = false;
                return response;
            }
        }
        delay(1);
    }
    modemBusy = false;
    Serial.println(F("\nAT command timeout"));
    return response;
}
bool checkRegistration(bool force = false)
{
    if (modemBusy)
        return networkRegistered;
    unsigned long now = millis();
    if (force || (now - lastCheck >= CHECK_INTERVAL))
    {
        lastCheck = now;
        String response = sendAT("AT+CREG?");
        bool registered =
            response.indexOf(",1") != -1 ||
            response.indexOf(",5") != -1;
        if (registered != networkRegistered)
        {
            networkRegistered = registered;
            Serial.println(
                registered ? 
                "Network Registered" : 
                "Network Lost"
            );
        }
    }
    return networkRegistered;
}
bool waitForPrompt(unsigned long timeout = 5000)
{
    unsigned long start = millis();

    while (millis() - start < timeout)
    {
        while(sim800.available())
        {
            char c = sim800.read();

            Serial.write(c);

            if(c == '>')
                return true;
        }
    }

    return false;
}

void processSIM800Char(char c) {
  Serial.print('[');
Serial.print((int)c);
Serial.print(']');
Serial.write(c);
    /*uint8_t b = (uint8_t)c;
    // Debug output
    Serial.print("RX: ");
    printHexByte(b);
    Serial.print(" ");
    if (b >= 32 && b <= 126)
        Serial.write(b);
    else
        Serial.print('.');*/
    Serial.println();
    // Ignore carriage return
    if (c == '\r') {
        return;
    }
    // Build the current line

    if (c != '\n') {
      //if (incomingLine.length() < 256) {
        //incomingLine += c;  //prevent it from growing forever if the SIM800 sends unexpected data
      if (incomingIndex < RX_BUFFER_SIZE - 1)
      {
            incomingBuffer[incomingIndex++] = c;
      }        
      return;
    }
    Serial.println("NEWLINE RECEIVED");

    // End of line reached
    incomingBuffer[incomingIndex] = '\0';
    incomingIndex = 0;

    String incomingLine = incomingBuffer;
    incomingLine.trim();

    if (incomingLine.length() == 0)
        return;

    Serial.print("LINE: ");
    Serial.println(incomingLine);
/*

    incomingLine.trim();
    if (incomingLine.length() == 0) {
        return;
    }
    Serial.println("LINE: " + incomingLine); */
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
pendingSMS = "Relay is ON";
sendSMSPending = true;      }
      else if (incomingLine == "OFF") {
        blinkMode = false;
        setRelay(false);
        printState();
pendingSMS = "Relay is OFF";
sendSMSPending = true;      }
      else if (incomingLine == "BLINK") {
        if (!blinkMode) {
        blinkMode = true;
        setRelay(true);
        previousMillis = millis();
        printState();
pendingSMS = "Blink ON";
sendSMSPending = true;        }
      }
      else if (incomingLine == "STOP") {
        blinkMode = false;
        setRelay(false);
        printState();
pendingSMS = "Blink OFF";
sendSMSPending = true;      }
else
{
    pendingSMS = "Unknown command";
    sendSMSPending = true;
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
    modemBusy = true;

    if (!networkRegistered)
    {
        modemBusy = false;
        return;
    }
    sim800.print("AT+CMGS=\"");
    sim800.print(phoneNumber);
    sim800.println("\"");
    if (!waitForPrompt()) {
        Serial.println("SMS prompt timeout");
                modemBusy = false;
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
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, 16, 17);
  delay(200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.println("Relay is on low");
  // Basic SIM800 setup
  Serial.println("Waiting for SIM800...");
  unsigned long start = millis();
  String response;
  while (millis() - start < 60000) {
    response = sendAT("AT");
    if (response.indexOf("OK") != -1)
    {
        modemReady = true;
        break;
    }
    delay(1000);
  }
  if (!modemReady)
  {
    Serial.println("SIM800 not responding");
    ESP.restart();  
        return;
  }
  Serial.println("SIM800 Ready");
  response = sendAT("AT+CPIN?");
  if (response.indexOf("READY") == -1)
  {
    Serial.println("SIM card not ready");
    ESP.restart();  
        return;
  }
  Serial.println("SIM card Ready");  
  sendAT("ATE0");      // Disable echo
  sendAT("AT+CMEE=2");      // verbose errors
  sendAT("AT+CMGF=1");      // CMGF means Cellular Message Format SMS text mode
  sendAT("AT+CSCS=\"GSM\""); //text mode SMS
  response = sendAT("AT+CSQ"); //Check signal quality
  Serial.println(response);
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

else
{    //sendAT("AT+CMGD=1,4");  //deletes all old sms

    sendAT("AT+CNMI=2,2,0,0,0");

    startupPhase = false;

    Serial.println("System Ready");
}
}
void loop(){
    // Check network status periodically
    bool networkOK = checkRegistration(false);
    unsigned long lastNoNetworkPrint = 0;
    // Read incoming SIM800 data
    if (networkOK && !modemBusy)
    {
        while (sim800.available())
        {
            char c = sim800.read();
            Serial.write(c);
            processSIM800Char(c);
        }
    }
else if (!networkOK)
{
    if (millis() - lastNoNetworkPrint > 5000)
    {
        Serial.println("Waiting for network");
        lastNoNetworkPrint = millis();
    }
}
    // Send queued SMS
    if (sendSMSPending && !modemBusy)
    {
        sendSMS(pendingSMS);

        pendingSMS = "";
        sendSMSPending = false;
    }
    // Relay blink handler
    if (blinkMode && millis() - previousMillis >= interval)
    {
        previousMillis = millis();
        setRelay(!relayState);
        printState();
    }
}
