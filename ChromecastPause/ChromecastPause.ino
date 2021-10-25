/*
 * SendDemo.cpp
 *
 * Demonstrates sending IR codes in standard format with address and command
 *
 *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
 *
 * Copyright (c) 2020-2021 Armin Joachimsmeyer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************************
 */

#include <Arduino.h>

/*
 * Define macros for input and output pin etc.
 */
#include "PinDefinitionsAndMore.h"

#define IR_RECEIVE_PIN      2
#define IR_SEND_PIN         8/////////////////////////////////////////////////////////////////////////
#define TONE_PIN            3
#define APPLICATION_PIN     5x
#define ALTERNATIVE_IR_FEEDBACK_LED_PIN 6x // E.g. used for examples which use LED_BUILDIN for example output.
#define IR_TIMING_TEST_PIN  7x



//#define EXCLUDE_EXOTIC_PROTOCOLS // saves around 240 bytes program space if IrSender.write is used
//#define SEND_PWM_BY_TIMER
//#define USE_NO_SEND_PWM
//#define EXCLUDE_UNIVERSAL_PROTOCOLS

//#define DECODE_DENON        // Includes Sharp
//#define DECODE_JVC
//#define DECODE_KASEIKYO
//#define DECODE_PANASONIC    // the same as DECODE_KASEIKYO
//#define DECODE_LG
//#define DECODE_NEC          // Includes Apple and Onkyo
//#define DECODE_SAMSUNG
#define DECODE_SONY
//#define DECODE_RC5
//#define DECODE_RC6
#define DECODE_ML

#define DEBUG

#include "IRremote.h"

#define DELAY_AFTER_SEND 2000
#define DELAY_AFTER_LOOP 2000

void setup() {
    Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)  || defined(ARDUINO_attiny3217)
    delay(1000); // To be able to connect Serial monitor after reset or power up and before first print out. Do not wait for an attached Serial Monitor!
#endif
    // Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));

    IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK); // Specify send pin and enable feedback LED at default feedback LED pin

    Serial.print(F("Ready to send IR signals at pin "));
    Serial.println(IR_SEND_PIN);

#if !defined(SEND_PWM_BY_TIMER) && !defined(USE_NO_SEND_PWM) && !defined(ESP32) // for esp32 we use PWM generation by ledcWrite() for each pin
    /*
     * Print internal signal generation info
     */
    IrSender.enableIROut(38);

//    Serial.print(F("Send signal mark duration is "));
//    Serial.print(IrSender.periodOnTimeMicros);
//    Serial.print(F(" us, pulse correction is "));
//    Serial.print(IrSender.getPulseCorrectionNanos());
//    Serial.print(F(" ns, total period is "));
//    Serial.print(IrSender.periodTimeMicros);
//    Serial.println(F(" us"));
#endif
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

    Serial.print(F("Ready to receive IR signals at pin "));
    Serial.println(IR_RECEIVE_PIN);

    Serial.setTimeout(100);
}

/*
 * Set up the data to be sent.
 * For most protocols, the data is build up with a constant 8 (or 16 byte) address
 * and a variable 8 bit command.
 * There are exceptions like Sony and Denon, which have 5 bit address.
 */
uint16_t sAddress = 0x96;
uint8_t sCommand = 0x1a;
uint8_t sRepeats = 0;

// address: 0x97 ommand: 0x19
#define PAUSE_RAWCOMMAND 0x4B99

// addressc: 0x97 ommand: 0x1A 
#define RESUME_RAWCOMMAND 0x4B9A 

uint8_t sPaused = 0;
uint8_t sPlayPauseReceived = 0;

void loop() {

//            IrSender.sendSony(0x97, 0x9A, 1, 15); //PLAY
//            IrSender.sendSony(0x97, 0x99, 1, 15); //PAUSE
//            IrSender.sendSony(0x97, 0x98, 1, 15); //STOP
//            sAddress++;
//            delay(200);

    if (IrReceiver.decode()) {
        Serial.println();
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
            IrReceiver.decodedIRData.flags = false; // yes we have recognized the flag :-)
            Serial.println(F("Overflow detected"));
            /*
             * do double beep
             */
            IrReceiver.stop();
            tone(TONE_PIN, 1100, 10);
            delay(50);

        } else {
            // Print a short summary of received data
            IrReceiver.printIRResultShort(&Serial);

            if (IrReceiver.decodedIRData.protocol == UNKNOWN ) {
                // We have an unknown protocol, print more info
                //IrReceiver.printIRResultRawFormatted(&Serial, true);
            }
        }

        if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
            /*
             * Play tone, wait and restore IR timer, if a valid protocol was received
             * Otherwise do not disturb the detection of the gap between transmissions. This will give
             * the next printIRResult* call a chance to report about changing the RECORD_GAP_MICROS value.
             */
            IrReceiver.stop();
            tone(TONE_PIN, 2200, 10);
            delay(8);
            IrReceiver.start(8000); // to compensate for 8 ms stop of receiver. This enables a correct gap measurement.
        }

        /*
         * !!!Important!!! Enable receiving of the next value,
         * since receiving has stopped after the end of the current received data packet.
         */
        IrReceiver.resume();

        /*
         * Finally check the received data and perform actions according to the received address and commands
         */

        sPlayPauseReceived = 0;
        
        if (!IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
          if (IrReceiver.decodedIRData.address == 0x52) {
              if (IrReceiver.decodedIRData.command == 0x36) {
                
                  sPlayPauseReceived = 1;
                  sPaused = !sPaused;
                  
              } else if (IrReceiver.decodedIRData.command == 0x45) {
                
                  sPlayPauseReceived = 1;
                  sPaused = 1;
              }
          }
        }
        
        if (sPlayPauseReceived) {
            if (sPaused) {
                Serial.write("PAUSE");
                IrReceiver.stop();
            delay(100);
            IrSender.sendSony(0x97, 0x99, 1, 15); //PAUSE
                
                IrReceiver.start();                
            } else {
                Serial.write("PLAY");
                IrReceiver.stop();                
            delay(100);
                IrSender.sendSony(0x97, 0x9A, 1, 15); //PLAY
                
                IrReceiver.start();
                
            }
        }

    }

    char input[5];
    int charsRead;
    int val;
    
    if (Serial.available() > 0) {
      charsRead = Serial.readBytesUntil('\n', input, 4);  // fetch the two characters
      input[charsRead] = '\0';                            // Make it a string
  
      val = StrToHex(input);                              // Convert it
  
      Serial.print("Hex input was: ");                    // Show it...
      Serial.print(input);
      Serial.print("   decimal = ");
      Serial.println(val);

      int address = val >> 8;
      int command = val & 0xFF;

//// command: 0x97 address: 0x19
//#define PAUSE_RAWCOMMAND 0x4B99
//
//// command: 0x97 address: 0x1A 
//#define RESUME_RAWCOMMAND 0x4B9A 

      if (address != 0 && command != 0) {
        IrReceiver.stop();
        
        Serial.print("Address: ");
        Serial.print(address);
        Serial.print("  Command: ");
        Serial.println(command);
        
        IrSender.sendSony(address, command, 4, 15);
        
        IrReceiver.start();
      }
    }

    
  
//    /*
//     * Print values
//     */
//    Serial.println();
//    Serial.print(F("address=0x"));
//    Serial.print(sAddress, HEX);
//    Serial.print(F(" command=0x"));
//    Serial.print(sCommand, HEX);
//    Serial.print(F(" repeats="));
//    Serial.println(sRepeats);
//    Serial.println();
//    Serial.println();
//    Serial.flush();
//
//
//    Serial.println(F("Send Sony/SIRCS with 7 command and 5 address bits"));
//    Serial.flush();
//    IrSender.sendSony(sAddress & 0x1F, sCommand & 0x7F, sRepeats);
//    //delay(DELAY_AFTER_SEND);
//    /*
//     * Increment values
//     * Also increment address just for demonstration, which normally makes no sense
//     */
//    sAddress += 0x0101;
//    sCommand += 0x11;
//    sRepeats++;
//    // clip repeats at 4
//    if (sRepeats > 6) {
//        sRepeats = 6;
//    }
//
//  int incomingByte = 0; // for incoming serial data
//
//    // send data only when you receive data:
//    if (Serial.available() > 0) {
//      // read the incoming byte:
//      incomingByte = Serial.read();
//  
//      // say what you got:
//      Serial.print("I received: ");
//      Serial.println(incomingByte, DEC);
//    }
//    
//    delay(DELAY_AFTER_LOOP); // additional delay at the end of each loop
}

int StrToHex(char str[])
{
  return (int) strtol(str, 0, 16);
}
