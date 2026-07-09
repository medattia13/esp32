// ESP32 LED Blink: 3 seconds ON, 3 seconds OFF

// Change this if your board uses a different LED pin
const int ledPin = 2;

void setup() {
    // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
}

void loop() {
    // put your main code here, to run repeatedly:

  digitalWrite(ledPin, HIGH);  // LED ON
  delay(3000);                 // Wait 3 seconds

  digitalWrite(ledPin, LOW);   // LED OFF
  delay(3000);                 // Wait 3 seconds
}
