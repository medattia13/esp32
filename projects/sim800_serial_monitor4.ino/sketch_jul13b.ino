#define TINY_GSM_MODEM_SIM800

#include <TinyGsmClient.h>

// Debug Serial Monitor
#define SerialMon Serial

// SIM800 UART
#define SerialAT Serial1

// Change these pins to match your wiring
#define MODEM_RX 16   // ESP32 RX  <- SIM800 TX
#define MODEM_TX 17   // ESP32 TX  -> SIM800 RX

TinyGsm modem(SerialAT);

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  SerialMon.println("Starting SIM800...");

  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  modem.restart();

  SerialMon.println("Modem Info:");
  SerialMon.println(modem.getModemInfo());

  if (modem.waitForNetwork()) {
    SerialMon.println("Network connected");
  } else {
    SerialMon.println("No network");
  }

  SerialMon.print("Signal Strength: ");
  SerialMon.println(modem.getSignalQuality());
}

void loop() {
  // Nothing here
}
