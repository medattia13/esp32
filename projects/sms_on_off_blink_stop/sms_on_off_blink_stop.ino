//TODO//
///remove delays is okay during startup, but a cleaner approach is to wait for responses from SIM800.
// add exception handling, eg esp initilization timer for relay time settings make sure old SMS are deleted
HardwareSerial sim800(2);

const int relayPin = 23;
// State changes variables
bool blinkMode = false;
bool relayState = false;
unsigned long previousMillis = 0;
const unsigned long interval = 3000;
const char phoneNumber[] = "98267980"; //C-style strings (better for memory) Your controller is going to run continuously. 
//A SIM800 relay controller might stay powered for months, so avoiding unnecessary String operations is better.
// Buffer for incoming SMS lines
String incomingLine = "";
//timer variables
bool timerEnabled = false;

int onHour = 18;
int onMinute = 30;

int offHour = 23;
int offMinute = 0;

int currentHour = 0;
int currentMinute = 0;

unsigned long lastTimeCheck = 0;
void getNetworkTime() {

  sim800.println("AT+CCLK?");

  delay(1000);

  while(sim800.available()) {

    String response = sim800.readString();

    Serial.println(response);

    int index = response.indexOf("\"");

    if(index > 0) {

      String datetime = response.substring(index+1,index+20);

      Serial.println("Time: " + datetime);

      currentHour = datetime.substring(9,11).toInt();
      currentMinute = datetime.substring(12,14).toInt();

      Serial.print("Hour:");
      Serial.println(currentHour);

      Serial.print("Minute:");
      Serial.println(currentMinute);
    }
  }
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
void sendSMS(String message) {

  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");
  delay(1000);
  sim800.print(message);
  delay(500);

  sim800.write(26);// CTRL+Z to send SMS
  delay(5000);
}
void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, 16, 17);
  delay(300);
  /* enabling time synchronisation
sim800.println("AT+CLTS=1"); //AT+CLTS=1 enables network time synchronization.
delay(1000);

sim800.println("AT&W");
delay(1000);*/
  // Basic SIM800 setup
  sim800.println("AT");
  delay(500);
  sim800.println("AT+CPIN?");
  delay(500);
  sim800.println("AT+CSQ"); //Check signal quality
  delay(500);
  sim800.println("AT+CREG?"); //Check network registration
  delay(500);
  sim800.println("AT+CMGF=1");      // CMGF means Cellular Message Format SMS text mode
  delay(500);
  sim800.println("AT+CNMI=2,2,0,0,0"); // Forward SMS directly to serial
  delay(500);
  Serial.println("System Ready");
}

void loop() {
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
      incomingLine.toUpperCase();
      /*if (incomingLine.startsWith("+")) {
        incomingLine = "";
        continue;
      }*/
      Serial.println("RX: " + incomingLine);
      // -----------------------------
      // Command handling
      // -----------------------------
      if (incomingLine == "ON") {
        blinkMode = false;
        relayState = true;
        digitalWrite(relayPin, HIGH);
        printState();
        sendSMS("Relay is ON");
      }
      else if (incomingLine == "OFF") {
        blinkMode = false;
        relayState = false;
        digitalWrite(relayPin, LOW);
        printState();
        sendSMS("Relay OFF");
      }
      else if (incomingLine == "BLINK") {
        blinkMode = true;
        relayState = true;
        digitalWrite(relayPin, HIGH);
        previousMillis = millis();
        printState();
        sendSMS("Blink mode started");
      }
      else if (incomingLine == "STOP") {
        blinkMode = false;
        relayState = false;
        digitalWrite(relayPin, LOW);
        printState();
        sendSMS("Blink mode stopped");
      }
      //used fo timer
       
       else if(incomingLine=="TIMER ON") {
        timerEnabled=true;
    sendSMS("Timer enabled");

    }

     else if(incomingLine=="TIMER OFF") {
 timerEnabled=false;
 sendSMS("Timer disabled");
}
else if(incomingLine=="TIME") {
 getNetworkTime();

 String msg="Time ";
 msg += String(currentHour);
 msg += ":";
 msg += String(currentMinute);

 sendSMS(msg);

}
      incomingLine = "";
    }
    else {
      if (incomingLine.length() < 100) {
    incomingLine += c;
      }
    }
  }
  // -----------------------------
  // Blink mode handler
  // -----------------------------
  if (blinkMode && millis() - previousMillis >= interval) {
      previousMillis = millis();
      relayState = !relayState;
      digitalWrite(relayPin, relayState);
      printState();
  }
  if(timerEnabled && millis() - lastTimeCheck > 60000){

  lastTimeCheck = millis();

  getNetworkTime();


  if(currentHour == onHour &&
     currentMinute == onMinute){

      relayState = true;
      digitalWrite(relayPin,HIGH);

      sendSMS("Timer: Relay ON");
  }


  if(currentHour == offHour &&
     currentMinute == offMinute){

      relayState = false;
      digitalWrite(relayPin,LOW);

      sendSMS("Timer: Relay OFF");
  }
}
}
