/*
  Mega analogWrite() test

  This sketch fades LEDs up and down one at a time on digital pins 2 through 13.
  This sketch was written for the Arduino Mega, and will not work on other boards.

  The circuit:
  - LEDs attached from pins 2 through 13 to ground.

  created 8 Feb 2009
  by Tom Igoe

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/AnalogWriteMega
*/

// These constants won't change. They're used to give names to the pins used:
const int lowestPin = 10;
const int highestPin = 12;
const uint8_t forDelayMS = 3;

void fadeLED(uint8_t pin, uint8_t delayMS) {
  // Fade the LED on pin from off to brightest:
  for (int brightness = 0; brightness < 255; brightness++) {
    analogWrite(pin, brightness);
    delay(delayMS);
  }

  // Fade the LED on pin from brightest to off:
  for (int brightness = 255; brightness >= 0; brightness--) {
    analogWrite(pin, brightness);
    delay(delayMS);
  }
}

void setup() {
  // set pins 2 through 13 as outputs:
  for (int thisPin = lowestPin; thisPin <= highestPin; thisPin++) {
    pinMode(thisPin, OUTPUT);
  }
}

void loop() {
  // Iterate over the pins:
  for (int thisPin = lowestPin; thisPin <= highestPin; thisPin++) {
    fadeLED(thisPin, 5);

    // Pause between LEDs:
    delay(100);
  }
}
