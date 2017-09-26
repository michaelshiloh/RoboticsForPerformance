/*********************************************************************
  Simple robot controlled by Adafruit Bluefruit module

  Based on Adafruit DCMotorTest and Adafruit Bluefruit controller examples
  - thanks Adafruit!
  Also based on functions used in my terriblySimpleRobot

  25 Sept 2017 - Michael Shiloh - Program creation


  Since I copied much code from Adafruit, I include their required text below:

  This is an example for our nRF51822 based Bluefruit LE modules

  Pick one up today in the adafruit shop!

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  MIT license, check LICENSE for more information
  All text above, and the splash screen below must be included in
  any redistribution
*********************************************************************/

#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
#include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h" // perhaps not necessary

#include "BluefruitConfig.h"

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

// Create the motor shield object from the Adafruit_MotorShield class
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

// Create pointers to two motor objects from the Adafruit_DCMotor class
Adafruit_DCMotor * rightMotor = AFMS.getMotor(1);
Adafruit_DCMotor * leftMotor = AFMS.getMotor(2);

/* See original Bluefruit controller example for an explanation */
// turning this off to avoid losing the firmware update every time.
#define FACTORYRESET_ENABLE         0
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

// Create the bluefruit object,

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// The amount of time to turn right. I made this global so that I could adjust
// and have it survive loops, but in general global variables like this are a bad
// idea. What's a better way to do this?
int rightTurnTime90Degrees;

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

// function prototypes over in packetparser.cpp
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

// the packet buffer
extern uint8_t packetbuffer[];


/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit App Controller Example"));
  Serial.println(F("-----------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      error(F("Couldn't factory reset"));
    }
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
    delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set Bluefruit to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));

  // Initialize the Adafruit Motor Shield
  AFMS.begin();  // create with the default frequency 1.6KHz

  // a guess at what might be a right turn
  rightTurnTime90Degrees = 300;

}

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/
void loop(void)
{
  /* Wait for new data to arrive */
  uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
  if (len == 0) return;

  /* Got a packet! */
  // printHex(packetbuffer, len);

  // Buttons
  if (packetbuffer[1] == 'B') {
    uint8_t buttnum = packetbuffer[2] - '0';
    boolean pressed = packetbuffer[3] - '0';
    Serial.print ("Button "); Serial.print(buttnum);
    if (pressed) {
      Serial.println(" pressed");
    } else {
      Serial.println(" released");
    }

    if (buttnum == 1 && pressed == true) {
      robotDoSquare();
    }

    if (buttnum == 5 && pressed == true) {
      // increase the right turn time
      rightTurnTime90Degrees += 10;
      Serial.print("rightTurnTime90Degrees increased to ");
      Serial.print(rightTurnTime90Degrees);
      Serial.println();
    }
    if (buttnum == 6 && pressed == true) {
      // decrease the right turn time
      rightTurnTime90Degrees -= 10;
      Serial.print("rightTurnTime90Degrees decreased to ");
      Serial.print(rightTurnTime90Degrees);
      Serial.println();
    }
  }
}

// Function to make the robot turn right
// for a certain amount of time
// This function stops the motor once the
// time is up
// Note that this function takes one parameter,
// namely, the amount of time in millliseconds
// to turn
void turnRight(int amount) { // amount is the parameter
  Serial.print("turnRight for ");
  Serial.print(amount);
  Serial.print("milliseconds");
  Serial.println();
  leftMotor->setSpeed(250);
  leftMotor->run(FORWARD);
  rightMotor->setSpeed(50);
  rightMotor->run(BACKWARD);
  delay (amount); // here we are using the parameter
  stop();
  delay (250);
}

// stop both motors
void stop() {
  Serial.println("stop");
  leftMotor->run(RELEASE);
  rightMotor->run(RELEASE);
}

// Go forward for a certain amount of time, stopping
// motors when the time is up
void goForward(int amount) {
  Serial.print("goForward for ");
  Serial.print(amount);
  Serial.print("milliseconds");
  Serial.println();

  leftMotor->setSpeed(150);
  leftMotor->run(FORWARD);
  rightMotor->setSpeed(150);
  rightMotor->run(FORWARD);
  delay (amount); // here we are using the parameter
  stop();
  delay (250);
}


// Try to do a square:
void robotDoSquare() {

  Serial.println("I will try to make a square");

  goForward(2000);

  turnRight(rightTurnTime90Degrees); // turn right for this much time

  goForward(2000);

  turnRight(rightTurnTime90Degrees); // rightTurnTime90Degrees is the argument

  goForward(2000);

  turnRight(rightTurnTime90Degrees);

  goForward(2000);
  stop();
}
