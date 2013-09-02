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
// Some of the one wire code came from terry@yourduino.com who based it on
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html
//
// A lot of the code for dealing with the ds18b20 devices came directly from
// the DallasTemperature library.  We pulled excerpts because we needed to save
// flash space.
//
// The ATTiny85 only has 8k of flash and 512bytes of RAM. These libraries are
// big but this still leaves us with 4k of flash left. If we were to use the
// DallasTemperature library we would only have about 100 bytes of flash left
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

// Since the ATTiny has no hardware uart we have to emulate one in software.
// The nastiest part is this costs us 2k of flash.
//
// XXX Maybe we need to bitbang our own serial to save space? Well, we should
//     have enough room.
//
SoftwareSerial mySerial(1,2); // RX, TX

// How many sensors do we have attached. We use an array to hold the uid's of
// the sensors we detect. Ideally we would 'malloc()' this array once. We have
// enough execution ram to hold the data. But bringing in malloc() is like over
// 1k of flash used so I am okay with hard coding a limit of 10 ds18b20's.
//
uint8_t num_sensors;
DeviceAddress dev[10];

// We are hardcoding assumptions for just _one_ wire device. All ds18b20's on
// this bus use the same resolution.
//
// used to requestTemperature with or without delay
bool waitForConversion;

// used to requestTemperature to dynamically check if a conversion is complete
//
bool checkForConversion;

// parasite power on or off
//
bool parasite;

// used to determine the delay amount needed to allow for the
// temperature conversion to take place
//
uint8_t bitResolution;



//************************************************************************
//
void setup(void) {

  // Start our soft serial port. Maybe we should use a higher baud rate so we
  // can talk to the eimp faster and thus spend less time 'awake' and more time
  // sleeping saving power.
  //
  mySerial.begin(9600);

  // Count the number of sensors and get their addresses
  //
  num_sensors = discoverOneWireDevices(oneWire, dev);
}

//************************************************************************
//
void loop(void) {
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


//************************************************************************
//
// function to print a device address
//
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) mySerial.print("0");
    mySerial.print(deviceAddress[i], HEX);
  }
}

//************************************************************************
//
void discoverOneWireDevices(OneWire ourBus, DeviceAddress *devices) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  uint8_t j;

  j = 0;
  while(ourBus.search(addr)) {
    devices[j] = addr;
    j++;
    printAddress(devices[j]);
  }

  return j;
}

//************************************************************************
//
// sends command for all devices on the bus to perform a temperature conversion
//
void requestTemperatures() {
  oneWire->reset();
  oneWire->skip();
  oneWire->write(STARTCONVO, parasite);

  // ASYNC mode?
  if (!waitForConversion) return;
  blockTillConversionComplete(&bitResolution, 0);

  return;
}

//************************************************************************
//
bool isConversionAvailable(uint8_t* deviceAddress) {
  // Check if the clock has been raised indicating the conversion is
  // complete
  //
  ScratchPad scratchPad;
  readScratchPad(deviceAddress, scratchPad);
  return scratchPad[0];
}

//************************************************************************
//
void blockTillConversionComplete(uint8_t* bitResolution,
                                 uint8_t* deviceAddress) {
  if(deviceAddress != 0 && checkForConversion && !parasite) {
    // Continue to check if the IC has responded with a temperature NB: Could
    // cause issues with multiple devices (one device may respond faster)
    //
    unsigned long start = millis();
    while(!isConversionAvailable(0) && ((millis() - start) < 750));
  }

  // Wait a fix number of cycles till conversion is complete (based on IC
  // datasheet)
  //
  switch (*bitResolution) {
  case 9:
    delay(94);
    break;
  case 10:
    delay(188);
    break;
  case 11:
    delay(375);
    break;
  case 12:
  default:
    delay(750);
    break;
  }
}

//************************************************************************
//
// attempt to determine if the device at the given address is connected to the
// bus
//
bool isConnected(uint8_t* deviceAddress)
{
  ScratchPad scratchPad;
  readScratchPad(deviceAddress, &scratchPad);
  return (oneWire->crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
}

//************************************************************************
//
// returns temperature in degrees C or DEVICE_DISCONNECTED if the device's
// scratch pad cannot be read successfully.
//
// NOTE: DEVICE_DISCONNECTED is defined as a large negative number outside the
//       operating range of the device.
//
float DallasTemperature::getTempC(uint8_t* deviceAddress)
{
  // TODO: Multiple devices (up to 64) on the same bus may take
  //       some time to negotiate a response
  // What happens in case of collision?

  ScratchPad scratchPad;
  if (isConnected(deviceAddress, scratchPad)) {
    return calculateTemperature(deviceAddress, scratchPad);
  }
  return DEVICE_DISCONNECTED;
}
