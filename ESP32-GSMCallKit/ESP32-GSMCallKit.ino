#include "src/SIM800.h"
SIM800 modem;

void setup() 
{
      Serial.begin(115200);
  modem.begin();
}
void loop() 
{
  modem.update();
 

}
