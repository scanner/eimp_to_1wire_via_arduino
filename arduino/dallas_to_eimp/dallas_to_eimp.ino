//************************************************************************
//
// A simple somewhat stripped down program to use an attiny85 to read from a
// series of ds18b20's and provide the output as a serial stream to an electric
// imp. The eimp has difficulty interacting with 1-wire devices because it is
// an interpreted language device. Granted, you can hack around it by futzing
// with the uart on the eimp but I needed to learn to work with attiny's in
// this fashion because I want to drive other bits of hardware from the eimp
// that requires the kind of bit banging that the 1-wire devices to (like other
// sensors and led's that require tight control of timing.
//
// This also forces us to learn how to put the attiny in as low power mode as
// possible when it is not being used by the eimp. We need to think about
// battery powered devices (and we do not want to replace the batteries every
// week.)
//

// The ATTiny85 only has 8k of flash and 512bytes of RAM. These libraries are
// big but this still leaves us with 4k of flash left. If we were to use the
// DaallasTemperature library we would only have about 100 bytes of flash left
// for our actual program logic so we are going to have to drive the ds18b20's
// by hand here.
//
#include <OneWire.h>
#include <SoftwareSerial.h>

// Data wire is plugged into pin 6 on the Arduino
#define ONE_WIRE_BUS 3

// Setup a oneWire instance to communicate with any OneWire devices (not just
// Maxim/Dallas temperature ICs)
//
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

SoftwareSerial mySerial(1,2); // RX, TX

uint8_t num_sensors;
DeviceAddress dev[10];

void setup(void)
{

  // start serial port
  mySerial.begin(9600);

  // Start up the library
  sensors.begin(); // IC Default 9 bit. If you have troubles consider upping it
                   // 12. Ups the delay giving the IC more time to process the
                   // temperature measurement

  num_sensors = sensors.getDeviceCount();
  for (uint8_t i = 0; i < num_sensors; i++ ) {
    sensors.getAddress(dev[i], i);
  }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) mySerial.print("0");
    mySerial.print(deviceAddress[i], HEX);
  }
}

void loop(void)
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures

  mySerial.print("T ");
  mySerial.println(num_sensors,DEC);
  for(uint8_t i = 0; i < num_sensors; i++ ) {
    printAddress(dev[i]);
    mySerial.print(" ");
    mySerial.println(sensors.getTempCByIndex(i));
  }
  mySerial.println("D");
  delay(1000);
}
