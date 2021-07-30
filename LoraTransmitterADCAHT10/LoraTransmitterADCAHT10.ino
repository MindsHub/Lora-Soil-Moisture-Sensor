// LoRa 9x_TX
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client (transmitter)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example LoRa9x_RX

//Function:Transmit Soil Moisture to Lora
//Author: Charlin
//Date:2020/06/12
//hardware: Lora Soil Moisture Sensor v1.1


#include <SPI.h>
#include "RH_RF95.h"

#include "I2C_AHT10.h"
#include <Wire.h>

#define MOISTURE_SENSOR_PIN A2 // select the input pin for the potentiometer
#define MOISTURE_SENSOR_POWER_CONTROL_PIN 5

#define RFM95_CS 10
#define RFM95_RST 4
#define RFM95_INT 2

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 868.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

AHT10 humiditySensor;

void setup()  {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, LOW);
  delay(100);
  digitalWrite(RFM95_RST, HIGH);

  pinMode(MOISTURE_SENSOR_POWER_CONTROL_PIN, OUTPUT);
  digitalWrite(MOISTURE_SENSOR_POWER_CONTROL_PIN, HIGH); //Sensor power on

  Serial.begin(115200);
  delay(100);

  Wire.begin(); //Join I2C bus

  //Check if the AHT10 will acknowledge
  if (!humiditySensor.begin()) {
    Serial.println("AHT10 NOT DETECTED. Please check wiring. Freezing.");
    while (1);
  }
  Serial.println("AHT10 acknowledged.");
  Serial.println("Marduino LoRa TX Test!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init FAILED. Freezing.");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  //Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
}


void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)

  if (!humiditySensor.available()) {
    delay(5000);
    return; // prevent reading invalid data
  }

  // get the new temperature, humidity and soil moisture values
  float temperature = humiditySensor.getTemperature();
  float humidity = humiditySensor.getHumidity();
  int sensorValue = analogRead(MOISTURE_SENSOR_PIN);

  // check if any reads failed and exit early (to try again)
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from AHT sensor!");
    return; // do not send invalid data
  }

  String message = "{\"H\":" + (String) humidity + ",\"T\":" + (String) temperature + ",\"M\":" + (String) sensorValue + "}";
  Serial.println(message);

  // Send a message to rf95_server
  uint8_t radioPacket[message.length() + 1];
  message.toCharArray(radioPacket, message.length() + 1);
  radioPacket[message.length() + 1] = '\0';

  Serial.print("Sending... ");
  rf95.send((uint8_t *)radioPacket, message.length() + 1);
  rf95.waitPacketSent();
  Serial.print("Timeout... ");
  rf95.waitAvailableTimeout(500);
  Serial.println("Done");

  // the code waiting for reply was removed
  delay(1000);
}
