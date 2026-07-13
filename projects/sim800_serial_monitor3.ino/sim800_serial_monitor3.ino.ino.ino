void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  delay(3000);
  test_sim800_module();
  send_SMS();
}
void loop() {
  updateSerial();
}
void test_sim800_module()
{
  Serial2.println("AT");
  updateSerial();
  Serial.println();
  Serial2.println("AT+CSQ");
  updateSerial();
  Serial2.println("AT+CCID");
  updateSerial();
  Serial2.println("AT+CREG?");
  updateSerial();
  Serial2.println("ATI");
  updateSerial();
  Serial2.println("AT+CBC");
  updateSerial();
}
void updateSerial()
{
  delay(5000);
  while (Serial.available())
  {
      delay(5000);

    Serial2.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
    delay(5000);

  while (Serial2.available())
  {
      delay(5000);

    Serial.write(Serial2.read());//Forward what Software Serial received to Serial Port
  }
}
void send_SMS()
{
  Serial.println("Sending AT"); // Configuring TEXT mode
    delay(5000);
  Serial2.println("AT"); // Configuring TEXT mode
  updateSerial();
  /*Serial2.println("AT+CMGS=\"+919804049270\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  Serial2.print("Circuit Digest"); //text content
  updateSerial();
Serial.println();
  Serial.println("Message Sent");
  Serial2.write(26);*/
}
