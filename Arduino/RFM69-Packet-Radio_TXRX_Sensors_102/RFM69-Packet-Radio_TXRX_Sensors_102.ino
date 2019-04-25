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
  Updated:  April 25rd, 2019
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

/********************* RFM69 Radio Setup **************************/
#include <RHReliableDatagram.h>
#include <RH_RF69.h>

// change addresses for each client board, any number :)
#define MY_ADDRESS                    102

// Where to send packets to!
#define DEST_ADDRESS                  103

// Change to 434.0 or other frequency, must match RX's freq!
#define RFM69_FREQ                    915.0

// For the Adafruit Feather M0 based boards
#define RFM69_CS                      8
#define RFM69_INT                     3
#define RFM69_RST                     4
#define RFM69_POWER_LEVEL             14

// Single instance of the radio driver
RH_RF69 rfm69(RFM69_CS, RFM69_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram rfm69_manager(rfm69, MY_ADDRESS);

uint8_t rfm69PowerLevel = RFM69_POWER_LEVEL;
/******************** End of RFM69 Radio Setup **********************/

#define BUTTON_A                      5
#define BUTTON_B                      6
#define BUTTON_C                      9

#define PACKET_LED                    10
#define ERROR_LED                     11
#define STATUS_LED                    12

#define DEFAULT_BRIGHTNESS            2
#define NUMBER_OF_SCROLL_LOOPS        1
#define DEFAULT_CHAR_WAIT_MS          60
#define DEFAULT_SCROLL_SPEED_MS       3000

Adafruit_SHT31 sht31d = Adafruit_SHT31();
Adafruit_IS31FL3731_Wing matrix = Adafruit_IS31FL3731_Wing();
Adafruit_TSL2591 tsl2591 = Adafruit_TSL2591(2591);

// Flags for found and initialized devices
bool tsl2591Found = true;
bool sht31Found = true;
bool is31DisplayFound = true;
bool rfm69RadioFound = true;

uint8_t brightness = DEFAULT_BRIGHTNESS;

bool pressed_A, pressed_B, pressed_C = false;

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

    if ((x - t) >= 0.5) {
      x += 1.0;
    }

    return int(x);
  } else {
     t = floor(-x);

     if (t + x <= -0.5)
       x += 1.0;

     return int(-x);
  }
}

void fadeBlinkLED(uint8_t pin, uint8_t delayMS) {
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

/*
    Performs a read using the Adafruit Unified Sensor API.
*/
double lightLevel(void) {
  // Get a new sensor event
  sensors_event_t event;
  tsl2591.getEvent(&event);

  if ((event.light == 0) | (event.light > 4294966000.0) |  (event.light <-4294966000.0)) {
    /* If event.light = 0 lux the sensor is probably saturated and no reliable data could be generated! */
    /* if event.light is +/- 4294967040 there was a float over/underflow */
    Serial.println(F("Invalid data (adjust gain or timing)"));
    return -3.141592657;
  } else {
    return event.light;
  }
}

void button_a_isr(void) {
  pressed_A = true; 
}

void button_b_isr(void) {
  pressed_B = true; 
}

void button_c_isr(void) {
  pressed_C = true; 
}
void setup() {
  Serial.begin(9600);
  delay(5000);
  
  Serial.println("RFM69 Radio, SHT31-D, TSL2591, and CharliePlex FeatherWing test");
  Serial.println("");

  // Initialize digital pins for LED outputs and button inputs.
  pinMode(PACKET_LED, OUTPUT);
  pinMode(ERROR_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
    
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // Initialize interrupts for the buttons
  attachInterrupt(digitalPinToInterrupt(BUTTON_A), button_a_isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_B), button_b_isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_C), button_c_isr, FALLING);

  // Initialize the SHT31-D temperature and humidity sensor
  if (sht31d.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("The SHT31-D temperature and humidity sensor was found.");
  } else {
    Serial.println("The SHT31-D temperature and humidity sensor was not found!");
    sht31Found = false;
  }
  
  // Initialize the TSL2591 light sensor
  if (tsl2591.begin())  {
    Serial.println(F("The TSL2591 light sensor was found."));

    // Configure the sensor
    tsl2591.setGain(TSL2591_GAIN_MED);      // 25x gain
    tsl2591.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  }  else {
    Serial.println(F("The TSL2591 light sensor was not found!"));
    tsl2591Found = false;
  }

  // Initualize the IS31 CharliePlexed matrix display
  if (matrix.begin()) {
    Serial.println("The IS31 display controller was found.");
  } else {
    Serial.println("The IS31 display controller was not found!");
    is31DisplayFound = false;
  }
    
  // Initialize the RFM69 packet radio
  if (rfm69_manager.init()) {
    Serial.println("Initialization of the RFM69 radio succeeded.");
 
    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
    // No encryption
    if (rfm69.setFrequency(RFM69_FREQ)) {    
      pinMode(RFM69_RST, OUTPUT);
      digitalWrite(RFM69_RST, LOW);
    
      // Manual reset
      digitalWrite(RFM69_RST, HIGH);
      delay(10);
      digitalWrite(RFM69_RST, LOW);
      delay(10);
      
      Serial.print("The RFM69 radio's frequency is ");
      Serial.print((int)RFM69_FREQ);
      Serial.println(" MHz");
  
      // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
      // ishighpowermodule flag set like this:
      rfm69.setTxPower(rfm69PowerLevel, true);
        
      Serial.print("The RFM69 radio's power level is ");
      Serial.println(rfm69PowerLevel);
    } else {
      Serial.println("Unable to set the RFM69 radio's frequency!");
      rfm69RadioFound = false;
    }
  } else {
    Serial.println("Initialization of the RFM69 radio failed!");
    rfm69RadioFound = false;
  }

  if (rfm69RadioFound) {
    Serial.println("The RFM69 packet radio was found.");
  } else {
    Serial.println("The RFM69 packet radio was not found!");
  }

  Serial.println("");
}

// Do not put this on the stack:
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
uint8_t data[] = "  OK";

void loop() {
  bool sht31DataValid = true;
  bool tsl2591DataValid = true;

  blinkLED(PACKET_LED, 25);
  
  fadeBlinkLED(ERROR_LED, 1);
  blinkLED(STATUS_LED, 200);

  if (tsl2591Found) {
    double lux = 0.0;
    long luxInt = 0;
  
    String lightStr = "";

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
      } else if ((luxInt >= 0) and (luxInt <= 20)) {
        brightness = 2;
      } else if ((luxInt > 20) and (luxInt <= 30)) {
        brightness = 3;
      } else if ((luxInt > 30) and (luxInt <= 70)) {
        brightness = 4;
      } else if ((luxInt > 70) and (luxInt <= 120)) {
        brightness = 6;
      } else if ((luxInt > 120) and (luxInt <= 250)) {
        brightness = 9;
      } else if ((luxInt > 250) and (luxInt <= 350)) {
        brightness = 12;
      } else if ((luxInt > 350) and (luxInt <= 450)) {
        brightness = 15;
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
        lightStr = "L = " + String(luxInt) + " lux, B = " + String(brightness);
      } else {
        tsl2591DataValid = false;
      }
    } else {
      tsl2591DataValid = false;
      Serial.println("Unable to automatically set brightness for the display!");
    }

    if (tsl2591DataValid) {
      Serial.print("Light = '");
      Serial.print(lightStr);
      Serial.print("', Length = ");
      Serial.println(lightStr.length());
            
      // Scroll the light level reading and brightness setting across the display
      if (is31DisplayFound and tsl2591DataValid) {
        printString(matrix, lightStr, brightness);         
        delay(1000);
        matrix.clear();
      } 
    }
  }

  if (sht31Found) {
    float celsius = sht31d.readTemperature();
    float fahrenheit = (celsius * 9/5) + 32;
    float humidity = sht31d.readHumidity();

    String temperatureStr =  "T = " + String(fahrenheit, 1) + " F (" + String(celsius, 1) + " C), H = " + String(humidity, 1) + "%";
    
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

  Serial.print("(1) Buttons: A = ");
  Serial.print(pressed_A?"Pressed":"Not pressed");
  Serial.print(", B = ");
  Serial.print(pressed_B?"Pressed":"Not pressed");
  Serial.print(", C = ");
  Serial.println(pressed_C?"Pressed":"Not pressed");
  Serial.println("");

  if (rfm69RadioFound) {
    if (rfm69_manager.available()) {
      // Wait for a message addressed to us from the client
      uint8_t len = sizeof(buf);
      uint8_t from;

      if (rfm69_manager.recvfromAck(buf, &len, &from)) {        
        buf[len] = 0;

        Serial.print("Got request from: 0x");
        Serial.print(from, HEX);
        Serial.print(": '");
        Serial.print((char*)buf);
        Serial.print("', RSSI: ");
        Serial.println(rfm69.lastRssi(), DEC);

        // Echo last button       
        data[0] = buf[11];

        // Send a reply back to the originating client
        if (!rfm69_manager.sendtoWait(data, sizeof(data), from)) {
          Serial.println("(1) sendtoWait failed!");
          Serial.println("");
        }
        
        digitalWrite(STATUS_LED, HIGH);
        delay(75);

        Serial.print("Reply:");
        Serial.println((char*)buf);
        Serial.print("RSSI: ");
        Serial.println(rfm69.lastRssi());

        digitalWrite(STATUS_LED, LOW);
        delay(75);
      }
    }

    if (pressed_A || pressed_B || pressed_C) {
      String sw_A = (pressed_A?"On":"Off");
      String sw_B = (pressed_B?"On":"Off");
      String sw_C = (pressed_C?"On":"Off");
      String switchStr = "A = " + sw_A + ", B = " + sw_B + ", C = " + sw_C;
      
      Serial.print("Switches: ");
      Serial.print(switchStr);
      Serial.print(", Length = ");
      Serial.println(switchStr.length());
      
      printString(matrix, switchStr, brightness);

      char radiopacket[20] = "Buttons:";

      Serial.println("Button(s) were pressed!");
    
      if (pressed_A) {
        radiopacket[8] = 'A';
      } else {
        radiopacket[8] = '-';
      }

      if (pressed_B) {
        radiopacket[9] = 'B';
      } else {
        radiopacket[9] = '-';
      }

      if (pressed_C) {
        radiopacket[10] = 'C';
      } else {
        radiopacket[10] = '-';
      }

      radiopacket[11] = 0;

      Serial.print("Sending '");
      Serial.print(radiopacket);
      Serial.print("', Length = ");
      Serial.println(strlen(radiopacket));

      if (rfm69_manager.sendtoWait((uint8_t *) radiopacket, strlen(radiopacket), DEST_ADDRESS)) {
        // Now wait for a reply from the server
        uint8_t len = sizeof(buf);
        uint8_t from;
  
        if (rfm69_manager.recvfromAckTimeout(buf, &len, 2000, &from)) {
          buf[len] = 0;
  
          Serial.print("Got reply from: 0x");
          Serial.print(from, HEX);
          Serial.print(": '");
          Serial.println((char*)buf);
          Serial.println("'");

          Serial.print("Reply:");
          Serial.println((char*)buf);
          Serial.print("RSSI: ");
          Serial.println(rfm69.lastRssi());
        } else {
          Serial.println("No reply, is anyone listening?");
        }
      } else {
        Serial.println("(2) sendtoWait failed");
        Serial.println("");
      }
      radiopacket[8] = radiopacket[9] = radiopacket[10] = 0;
    }
  }

  // Reset all the push buttons
  Serial.println("Resetting the buttons");

  pressed_A = false;
  pressed_B = false;
  pressed_C = false;

  digitalWrite(BUTTON_A, HIGH);
  digitalWrite(BUTTON_B, HIGH);
  digitalWrite(BUTTON_C, HIGH);
  
  Serial.print("(2) Buttons: A = ");
  Serial.print(pressed_A?"Pressed":"Not pressed");
  Serial.print(", B = ");
  Serial.print(pressed_B?"Pressed":"Not pressed");
  Serial.print(", C = ");
  Serial.println(pressed_C?"Pressed":"Not pressed");
  Serial.println("");
}
