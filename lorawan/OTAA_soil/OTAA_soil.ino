#define M_MOSI 11
#define M_MISO 12
#define M_SCLK 13
#define M_CS 10
#define M_DIO0 2
#define M_DIO1 6
#define M_DIO2 7
#define M_DIO5 8
#define M_RST 9

// OTAA credentials
const char *devEui = "D896E0FF00000888";
const char *appEui = "70B3D57ED0041DA0";
const char *appKey = "DBA8038077B142A73CFAD1EB2F0C8FC4";

#include <SPI.h>
#include "RH_RF95.h"

#include "I2C_AHT10.h"
#include <Wire.h>
AHT10 humiditySensor;

int sensorPin = A2;  // select the input pin for the potentiometer
int sensorValue = 0; // variable to store the value coming from the sensor
int sensorPowerCtrlPin = 5;

int16_t packetnum = 0;   // packet counter, we increment per xmission
float temperature = 0.0; //
float humidity = 0.0;

/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the The Things Network.
 *
 * This uses OTAA (Over-the-air activation), where where a DevEUI and
 * application key is configured, which are used in an over-the-air
 * activation procedure where a DevAddr and session keys are
 * assigned/generated for use with all further communication.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!

 * To use this sketch, first register your application and device with
 * the things network, to set or generate an AppEUI, DevEUI and AppKey.
 * Multiple devices can use the same AppEUI, but each device has its own
 * DevEUI and AppKey.
 *
 * Do not forget to define the radio type correctly in config.h.
 *
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
//70B3D57ED0041DA0
//static const u1_t PROGMEM APPEUI[8]={ 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x04, 0x1D, 0xA0 };
static const u1_t PROGMEM APPEUI[8] = {0xA0, 0x1D, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70};
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }

// This should also be in little endian format, see above.
//D896E0FF00000888
//static const u1_t PROGMEM DEVEUI[8]={ 0xD8, 0x96, 0xE0, 0xFF, 0x00, 0x00, 0x08, 0x88 };
static const u1_t PROGMEM DEVEUI[8] = {0x88, 0x08, 0x00, 0x00, 0xFF, 0xE0, 0x96, 0xD8};
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
//DBA8 0380 77B1 42A7 3CFA D1EB 2F0C 8FC4
static const u1_t PROGMEM APPKEY[16] = {0xDB, 0xA8, 0x03, 0x80, 0x77, 0xB1, 0x42, 0xA7, 0x3C, 0xFA, 0xD1, 0xEB, 0x2F, 0x0C, 0x8F, 0xC4};
//static const u1_t PROGMEM APPKEY[16] = { 0xC4, 0x8F, 0x0C, 0x2F, 0xEB, 0xD1, 0xFA, 0x3C, 0xA7, 0x42, 0xB1, 0x77, 0x80, 0x03, 0xA8, 0xDB };
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

static uint8_t mydata[] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = M_CS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = M_RST,
    .dio = {M_DIO0, M_DIO1, M_DIO2},
};

void onEvent(ev_t ev)
{
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev)
    {
    case EV_SCAN_TIMEOUT:
        Serial.println(F("EV_SCAN_TIMEOUT"));
        break;
    case EV_BEACON_FOUND:
        Serial.println(F("EV_BEACON_FOUND"));
        break;
    case EV_BEACON_MISSED:
        Serial.println(F("EV_BEACON_MISSED"));
        break;
    case EV_BEACON_TRACKED:
        Serial.println(F("EV_BEACON_TRACKED"));
        break;
    case EV_JOINING:
        Serial.println(F("EV_JOINING"));
        break;
    case EV_JOINED:
        Serial.println(F("EV_JOINED"));

        // Disable link check validation (automatically enabled
        // during join, but not supported by TTN at this time).
        LMIC_setLinkCheckMode(0);
        break;
    case EV_RFU1:
        Serial.println(F("EV_RFU1"));
        break;
    case EV_JOIN_FAILED:
        Serial.println(F("EV_JOIN_FAILED"));
        break;
    case EV_REJOIN_FAILED:
        Serial.println(F("EV_REJOIN_FAILED"));
        break;
        break;
    case EV_TXCOMPLETE:
        Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
        if (LMIC.txrxFlags & TXRX_ACK)
            Serial.println(F("Received ack"));
        if (LMIC.dataLen)
        {
            Serial.println(F("Received "));
            Serial.println(LMIC.dataLen);
            Serial.println(F(" bytes of payload"));
        }
        // Schedule next transmission
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
        break;
    case EV_LOST_TSYNC:
        Serial.println(F("EV_LOST_TSYNC"));
        break;
    case EV_RESET:
        Serial.println(F("EV_RESET"));
        break;
    case EV_RXCOMPLETE:
        // data received in ping slot
        Serial.println(F("EV_RXCOMPLETE"));
        break;
    case EV_LINK_DEAD:
        Serial.println(F("EV_LINK_DEAD"));
        break;
    case EV_LINK_ALIVE:
        Serial.println(F("EV_LINK_ALIVE"));
        break;
    default:
        Serial.println(F("Unknown event"));
        break;
    }
}

void do_send(osjob_t *j)
{
    readSensor();
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND)
    {
        Serial.println(F("OP_TXRXPEND, not sending"));
    }
    else
    {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, mydata, sizeof(mydata) - 1, 0);
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void readSensor()
{
    sensorPowerOn(); //
    delay(100);
    sensorValue = analogRead(sensorPin);
    delay(200);

    if (humiditySensor.available() == true)
    {
        //Get the new temperature and humidity value
        temperature = humiditySensor.getTemperature();
        humidity = humiditySensor.getHumidity();

        //Print the results
        Serial.print("Temperature: ");
        Serial.print(temperature, 2);
        Serial.print(" C\t");
        Serial.print("Humidity: ");
        Serial.print(humidity, 2);
        Serial.println("% RH");
    }
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature))
    {
        Serial.println(F("Failed to read from AHT sensor!"));
        //return;
    }

    delay(100);
    //sensorPowerOff();

    Serial.print(F("Moisture ADC : "));
    Serial.println(sensorValue);

    mydata[0] = uint8_t(int(temperature));
    mydata[1] = uint8_t(int(temperature * 10) % 10);
    mydata[2] = uint8_t(int(humidity));
    mydata[3] = uint8_t(sensorValue >> 8);
    mydata[4] = uint8_t((sensorValue & 0xff00) >> 8);
}

void sensorPowerOn(void)
{
    digitalWrite(sensorPowerCtrlPin, HIGH); //Sensor power on
}
void sensorPowerOff(void)
{
    digitalWrite(sensorPowerCtrlPin, LOW); //Sensor power on
}

void setup()
{
    pinMode(sensorPowerCtrlPin, OUTPUT);
    //digitalWrite(sensorPowerCtrlPin, LOW);//Sensor power on
    sensorPowerOn();
    Serial.begin(115200);

    Wire.begin(); //Join I2C bus
    //Check if the AHT10 will acknowledge
    if (humiditySensor.begin() == false)
    {
        Serial.println("AHT10 not detected. Please check wiring. Freezing.");
        //while (1);
    }
    else
        Serial.println("AHT10 acknowledged.");
    readSensor();
    
    Serial.println(F("Starting"));

#ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
#endif

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);
}

void loop()
{
    os_runloop_once();
}
