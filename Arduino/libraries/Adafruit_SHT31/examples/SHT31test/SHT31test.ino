/*************************************************** 
  This is an example for the SHT31-D Humidity & Temp Sensor

  Designed specifically to work with the SHT31-D sensor from Adafruit
  ----> https://www.adafruit.com/products/2857

  These sensors use I2C to communicate, 2 pins are required to  
  interface
 ****************************************************/
 
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(9600);

  //while (!Serial)
  //delay(10);     // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("SHT31 test");
  
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Could not find the SHT31!");

    //while (1) {
    //  delay(1);
    }
  }
}

void loop() {
  float celsius = sht31.readTemperature();
  float fahrenheit = (celsius * 0.8) + 32;
  float humidity = sht31.readHumidity();

  Serial.print("Temperature: ");
  Serial.print(fahrenheit);
  Serial.print("F, ");
  Serial.print(celsius);
  Serial.print("C, ");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("\%");

  delay(2000);
}
