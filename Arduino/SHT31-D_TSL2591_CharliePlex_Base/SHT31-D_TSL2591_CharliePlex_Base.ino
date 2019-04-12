/**************************************************************************** 
  This is a sketch for a small robust little sensor node that consists of:
  
    Feather M0 RFM69HCW Packet Radio - 868 or 915 MHz - RadioFruit.
      (https://www.adafruit.com/product/3176),
    SHT31-D Humidity & Temp Sensor
      (https://www.adafruit.com/products/2857),
    TSL2591 High Dynamic Range Digital Light Sensor
      (https://www.adafruit.com/product/1980),
    15x7 CharliePlex LED Matrix Display FeatherWing
      (https://www.adafruit.com/product/3134) Red or
      (https://www.adafruit.com/product/3135) Yellow or
      (https://www.adafruit.com/product/3136) Green or
      (https://www.adafruit.com/product/3137) Blue or
      (https://www.adafruit.com/product/3138) Cool White or
      (https://www.adafruit.com/product/3163) Warm White
    and two 10mm LEDs - Green (Good Packet sent or received) and Red (Error)

  The Feather board uses I2C to communicate with the sensors and FeatherWing.
  Just two pins are required for the interface. The code is based heavily on
    several Adafruit example sketches, with specific code added by me, and uses
    Adafruit libraries.

  Even if one or more directly connected components fails, this node will still
    be able to phone home for help and send status in formation to the server. Code
    that uses a specific component will only be executed IF the components
    exists and could be initialized.

  I have a brute force automatic display brightness control working now.

  Author:   Dale Weber <hybotics@hybotics.org>
  Date:     March 15th, 2019
  Updated:  April 9th, 2019
****************************************************************************/

/*
  Please support Adafruit, and buy a lot of stuff from them! They put in many 
    hours of hard work developing and making the products we use to make awesome
    projects.

  This project would NOT be happening if it were not for Adafruit and their
    AWESOME support staff and user forums.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>

#define BUTTON_A                      5
#define BUTTON_B                      6
#define BUTTON_C                      9

#define PACKET_LED                    10
#define ERROR_LED                     11
#define STATUS_LED                    12

#define DEFAULT_BRIGHTNESS            1
#define NUMBER_OF_SCROLL_LOOPS        2
#define DEFAULT_CHAR_WAIT_MS          60
#define DEFAULT_SCROLL_SPEED_MS       3000

Adafruit_SHT31 sht31d = Adafruit_SHT31();
Adafruit_IS31FL3731_Wing matrix = Adafruit_IS31FL3731_Wing();
Adafruit_TSL2591 tsl2591 = Adafruit_TSL2591(2591);

// Flags for found and initialized devices
bool tsl2591Found = true;
bool sht31Found = true;
bool is31DisplayFound = true;

uint8_t brightness = DEFAULT_BRIGHTNESS;

/****************************************************************
  These utility routines make this node go round and round.
****************************************************************/

// Round a float or double to an integer
int16_t roundToInt(double x) {
  double t;

  if (!isfinite(x))
    return (x);

  if (x >= 0.0) {
    t = floor(x);
/*
    Serial.print("x = ");
    Serial.print(x);
    Serial.print(", t = ");
    Serial.print(t);
    Serial.print(", (x - t) = ");
    Serial.println(x - t);
*/
    if ((x - t) >= 0.5) {
      x += 1.0;
/*
      Serial.print("rounded x = ");
      Serial.println(x);
*/
    }

    return int(x);
  } else {
     t = floor(-x);

     if (t + x <= -0.5)
       x += 1.0;

     return int(-x);
  }
}

// Blink an LED for waitMS ms.
void blinkLED(uint8_t ledPin, uint8_t waitMS) {
  digitalWrite(ledPin, HIGH);           // Turn the LED on (HIGH is the voltage level)
  delay(waitMS);                        // Wait for a period of time in ms
  digitalWrite(ledPin, LOW);            // Turn the LED off by making the voltage LOW
  delay(waitMS);                        // Wait for a period of time in ms
}

// Scroll a string, up to 21 characters long, across a CharliePlex FeatherWing
void printString(Adafruit_IS31FL3731_Wing mat, String str, uint8_t brightness = DEFAULT_BRIGHTNESS, uint8_t nrTimes = NUMBER_OF_SCROLL_LOOPS, uint8_t charWaitMS = DEFAULT_CHAR_WAIT_MS, uint8_t scrollDelayMS = DEFAULT_SCROLL_SPEED_MS) {
  uint8_t strLength = str.length();
  
  mat.setTextSize(1);
  mat.setTextWrap(false);               // We dont want text to wrap so it scrolls nicely
  mat.setTextColor(brightness);

  for (uint8_t i = 0; i < nrTimes; i++) {
    for (int16_t x = 0; x >= -(strLength * 6); x--) {
      mat.clear();
      mat.setCursor(x, 0);
      mat.print(str);

      delay(charWaitMS);
    }

    delay(scrollDelayMS);
  }  
}

/**************************************************************************/
/*
    Performs a read using the Adafruit Unified Sensor API.
*/
/**************************************************************************/
double lightLevel(void) {
  /* Get a new sensor event */ 
  sensors_event_t event;
  tsl2591.getEvent(&event);
 
  /* Display the results (light is measured in lux) */
/*  
  Serial.print(F("[ "));
  Serial.print(event.timestamp);
  Serial.print(F(" ms ] "));
*/  
  if ((event.light == 0) | (event.light > 4294966000.0) |  (event.light <-4294966000.0)) {
    /* If event.light = 0 lux the sensor is probably saturated */
    /* and no reliable data could be generated! */
    /* if event.light is +/- 4294967040 there was a float over/underflow */
    Serial.println(F("Invalid data (adjust gain or timing)"));
    return -3.141592657;
  } else {
    return event.light;
/*
    Serial.print("Light level = ");
    Serial.print(event.light, DEC);
    Serial.println(F(" lux"));
*/
  }
}

void setup() {
  Serial.begin(9600);
  delay(5000);
  
  Serial.println("Base SHT31-D, TSL2591, and CharliePlex FeatherWing test");
  Serial.println("");

  // Initialize digital pins for LED outputs and button inputs.
  pinMode(PACKET_LED, OUTPUT);
  pinMode(ERROR_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
    
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  
  if (sht31d.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("The SHT31-D temperature and humidity sensor was found.");
  } else {
    Serial.println("The SHT31-D temperature and humidity sensor was not found!");
    sht31Found = false;
  }
  
  if (tsl2591.begin())  {
    Serial.println(F("The TSL2591 light sensor was found."));

    // Configure the sensor
    tsl2591.setGain(TSL2591_GAIN_MED);      // 25x gain
    tsl2591.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  }  else {
    Serial.println(F("The TSL2591 light sensor was not found!"));
    tsl2591Found = false;
  }

  if (matrix.begin()) {
    Serial.println("The IS31 display controller was found.");
  } else {
    Serial.println("The IS31 display controller was not found!");
    is31DisplayFound = false;
  }
  
  Serial.println("");

  delay(5000);
}

void loop() {
  bool sht31DataValid = true;
  bool tsl2591DataValid = true;

  bool pressed_A, pressed_B, pressed_C = false;

  blinkLED(PACKET_LED, 50);
  blinkLED(ERROR_LED, 50);
  blinkLED(STATUS_LED, 150);

  if (sht31Found) {
    float celsius = sht31d.readTemperature();
    float fahrenheit = (celsius * 9/5) + 32;
    float humidity = sht31d.readHumidity();

    double lux = 0.0;
    long luxInt = 0;
  
    String temperatureStr =  "T=" + String(roundToInt(fahrenheit)) + "F(" + String(roundToInt(celsius)) + "C), H=" + String(humidity,  1) + "%";
    String lightStr = "";
    
    if (isnan(celsius)) {  // check if 'is not a number'
      Serial.println("Failed to read the temperature!");
      sht31DataValid = false;
    } else { 
      Serial.print("Temperature = ");
      Serial.print(fahrenheit);
      Serial.print("F (");
      Serial.print(celsius);
      Serial.print("C)");
      
      if (isnan(humidity)) {  // check if 'is not a number'
        Serial.println(""); 
        Serial.println("Failed to read the humidity!");
        sht31DataValid = false;
      } else {
        Serial.print(", Humidity = ");
        Serial.print(humidity);
        Serial.println("\%");
       
        // Get the light level reading and try to set the display brightness
        if (tsl2591Found) {
          // Check the light level and adjust the display brightness
          lux = lightLevel();
          luxInt = roundToInt(lux);

          if (lux > 0.0) {
            Serial.println("  ***   Setting the display brightness   ***");
          }
            
          if (lux <= -3.14) {
            luxInt = 0;
            brightness = 100;
            tsl2591DataValid = false;
            lightStr = "Unable to set brightness!";
          } else if (luxInt <= 20) {
            brightness = 1;
          } else if ((luxInt > 20) and (luxInt <= 30)) {
            brightness = 3;
          } else if ((luxInt > 30) and (luxInt <= 70)) {
            brightness = 6;
          } else if ((luxInt > 70) and (luxInt <= 120)) {
            brightness = 9;
          } else if ((luxInt > 120) and (luxInt <= 250)) {
            brightness = 12;
          } else if ((luxInt > 250) and (luxInt <= 350)) {
            brightness = 15;
          } else if ((luxInt > 350) and (luxInt <= 450)) {
            brightness = 18;
          } else if ((luxInt > 450) and (luxInt <= 550)) {
            brightness = 20;
          } else if ((luxInt > 550) and (luxInt <= 650)) {
            brightness = 25;
          } else if ((luxInt > 650) and (luxInt <= 750)) {
            brightness = 30;
          } else if ((luxInt > 750) and (luxInt <= 850)) {
            brightness = 35;
          } else if ((luxInt > 850) and (luxInt <= 950)) {
            brightness = 40;
          } else if ((luxInt > 950) and (luxInt <= 1050)) {
            brightness = 45;
          } else if ((luxInt > 1050) and (luxInt <= 1150)) {
            brightness = 50;
          } else if ((luxInt > 1150) and (luxInt <= 1250)) {
            brightness = 55;
          } else if ((luxInt > 1250) and (luxInt <= 1350)) {
            brightness = 60;
          } else if ((luxInt > 1350) and (luxInt <= 1450)) {
            brightness = 65;
          } else if ((luxInt > 1450) and (luxInt <= 1550)) {
            brightness = 70;
          } else if ((luxInt > 1550) and (luxInt <= 1650)) {
            brightness = 80;
          } else if ((luxInt > 1650) and (luxInt <= 1750)) {
            brightness = 90;
          } else if ((luxInt > 1750) and (luxInt <= 1850)) {
            brightness = 100;
          } else if ((luxInt > 1850) and (luxInt <= 2000)) {
            brightness = 110;
          } else if ((luxInt > 2000) and (luxInt <= 2400)) {
            brightness = 120;
          } else if (luxInt > 2400) {
            brightness = 130;
          }

          Serial.print("Light level = ");
          Serial.print(lux, DEC);
          Serial.print(" lux");
          Serial.print(", Brightness = ");
          Serial.println(brightness);

          if (lux > 0.0) {
            lightStr = "L=" + String(luxInt) + " lux, B=" + String(brightness);
          } else {
            tsl2591DataValid = false;
          }
        } else {
          tsl2591DataValid = false;
          Serial.println("Unable to automatically set brightness for the display!");
        }
          
        Serial.print("Temperature = '");
        Serial.print(temperatureStr);
        Serial.print("', Length = ");
        Serial.println(temperatureStr.length());

        // This is for TESTING ONLY
        //brightness = 8;

        // Scroll the temperature and readings across the display
        if (is31DisplayFound and sht31DataValid) {
          printString(matrix, temperatureStr, brightness);
          delay(1000);
          matrix.clear();
        }
          
        if (tsl2591Found) {
          Serial.print("Light = '");
          Serial.print(lightStr);
          Serial.print("', Length = ");
          Serial.println(lightStr.length());
            
          // Scroll the light level reading and brightness setting across the display
          if (is31DisplayFound and tsl2591DataValid) {
            printString(matrix, lightStr, brightness);         
            delay(2000);
            matrix.clear();
          } 
        }
      }
    }
    
    Serial.println();
  } else {
    Serial.println("There is no SHT31-D sensor to get data from!");
    sht31Found = false;
    sht31DataValid = false;

    tsl2591Found = false;
    tsl2591DataValid = false;
  }

  Serial.println("You have 5 seconds to push and hold the button(s).");
  delay(5000);
  
  // Read the push buttons - active LOW
  pressed_A = (digitalRead(BUTTON_A) == LOW);
  pressed_B = (digitalRead(BUTTON_B) == LOW);
  pressed_C = (digitalRead(BUTTON_C) == LOW);

  Serial.print("Buttons: A = ");
  Serial.print(pressed_A?"Pressed":"Not pressed");
  Serial.print(", B = ");
  Serial.print(pressed_B?"Pressed":"Not pressed");
  Serial.print(", C = ");
  Serial.println(pressed_C?"Pressed":"Not pressed");
  Serial.println("");

  // Reset all the push buttons
  pressed_A = false;
  pressed_B = false;
  pressed_C = false;
}
