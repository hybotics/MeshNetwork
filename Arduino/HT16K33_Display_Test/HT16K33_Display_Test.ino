// Demo the quad alphanumeric display LED backpack kit
// scrolls through every character, then scrolls Serial
// input onto the display

#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();

void writeString(Adafruit_AlphaNum4 alpha, String str, uint16_t charWaitMS = 10, uint8_t brightness = 1) {
  char displaybuffer[4] = {' ', ' ', ' ', ' '};
  uint16_t index = 0;

  alpha.setBrightness(brightness);
  
  for (index = 0; index < str.length(); index++) {
    if (isprint(str[index])) {    
      // Scroll down display
      displaybuffer[0] = displaybuffer[1];
      displaybuffer[1] = displaybuffer[2];
      displaybuffer[2] = displaybuffer[3];
      displaybuffer[3] = str[index];
     
      // Set every digit to the buffer
      alpha.writeDigitAscii(0, displaybuffer[0]);
      alpha.writeDigitAscii(1, displaybuffer[1]);
      alpha.writeDigitAscii(2, displaybuffer[2]);
      alpha.writeDigitAscii(3, displaybuffer[3]);
     
      // Write it out!
      alpha.writeDisplay();
      delay(200);
    }

    delay(charWaitMS);
  }
}

void setup() {
  alpha4.begin(0x70);
  alpha4.setBrightness(9);  
  
  alpha4.clear();
  alpha4.writeDisplay();
}

void loop() {
  char inChar = 0;
  String str = "This is a test of the Emergency Broadcasting System!";  

  alpha4.clear();
  alpha4.writeDisplay();

  writeString(alpha4, str, 50);
  
  delay(2000);
}
