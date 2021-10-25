/*
 * ir_ML.cpp
 *
 *  Contains functions for receiving and sending ML (HMB) Protocols
 *
 *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
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

//#define DEBUG // Activate this for lots of lovely debug output from this decoder.
//#define TRACE // Activate this for more debug output from this decoder.
#include "IRremoteInt.h" // evaluates the DEBUG for DEBUG_PRINT
#include "LongUnion.h"

/** \addtogroup Decoder Decoders and encoders for different protocols
 * @{
 */
//bool sLastSendToggleValue = false;
//uint8_t sLastReceiveToggleValue = 3; // 3 -> start value

//==============================================================================
//     ML (56kHz 23bit) 
//     18 56kHz cycles per mark
//==============================================================================
//
// see: https://www.sbprojects.net/knowledge/ir/rc5.php
// https://en.wikipedia.org/wiki/Manchester_code
// mark->space => 0
// space->mark => 1
// MSB first 1 start bit, 1 field bit, 1 toggle bit + 5 bit address + 6 bit command (6 bit command plus one field bit for RC5X), no stop bit
// duty factor is 25%,
//
#define ML_ADDRESS_BITS    14
#define ML_REPEAT_BIT       1
#define ML_COMMAND_BITS	  	8

#define ML_BITS            (ML_COMMAND_BITS + ML_REPEAT_BIT + ML_ADDRESS_BITS) // 23

#define ML_UNIT            320 // (18 cycles of 56 kHz)

#define MIN_ML_MARKS       ((ML_BITS + 1) / 2) // 12
#define ML_DURATION        (ML_BITS * 2 * ML_UNIT) // 14720
#define ML_REPEAT_PERIOD   (128L * ML_UNIT) // 113792
#define ML_REPEAT_SPACE    (ML_REPEAT_PERIOD - ML_DURATION) // 100 ms


/**
 * Try to decode data as RC5 protocol
 *                             _   _   _   _   _   _   _   _   _   _   _   _   _
 * Clock                 _____| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |
 *                                ^   ^   ^   ^   ^   ^   ^   ^   ^   ^   ^   ^    End of each data bit period
 *                               _   _     - Mark
 * 2 Start bits for RC5    _____| |_| ...  - Data starts with a space->mark bit
 *                                         - Space
 *                               _
 * 1 Start bit for RC5X    _____| ...
 *
 */
bool IRrecv::decodeML() {
    uint8_t tBitIndex;
    uint32_t tDecodedRawData = 0;

    // Set Biphase decoding start values
    initBiphaselevel(1, ML_UNIT); // Skip gap space

    // Check we have the right amount of data (11 to 26). The +2 is for initial gap and start bit mark.
    if (decodedIRData.rawDataPtr->rawlen < MIN_ML_MARKS + 2 && decodedIRData.rawDataPtr->rawlen > ((2 * ML_BITS) + 2)) {
        // no debug output, since this check is mainly to determine the received protocol
        DEBUG_PRINT(F("ML: "));
        DEBUG_PRINT("Data length=");
        DEBUG_PRINT(decodedIRData.rawDataPtr->rawlen);
        DEBUG_PRINTLN(" is not between 11 and 26");
        return false;
    }

// Check start bit, the first space is included in the gap
    // if (getBiphaselevel() != MARK) {
		// Serial.println("ML no mark");
        // DEBUG_PRINT(F("ML: "));
        // DEBUG_PRINTLN("first getBiphaselevel() is not MARK");
        // return false;
    // }

    /*
     * Get data bits - MSB first
     */
    for (tBitIndex = 0; sBiphaseDecodeRawbuffOffset < decodedIRData.rawDataPtr->rawlen; tBitIndex++) {
		DEBUG_PRINT("bit:");
		DEBUG_PRINTLN(tBitIndex);
        uint8_t tStartLevel = getBiphaselevel();
        uint8_t tEndLevel = getBiphaselevel();
		DEBUG_PRINT(tStartLevel);
		DEBUG_PRINTLN(tEndLevel);

        if ((tStartLevel == SPACE) && (tEndLevel == MARK)) {
            // we have a space to mark transition here
            tDecodedRawData = (tDecodedRawData << 1) | 0;
        } else if ((tStartLevel == MARK) && (tEndLevel == SPACE)) {
            // we have a mark to space transition here
            tDecodedRawData = (tDecodedRawData << 1) | 1;
        } else {
            // TRACE_PRINT since I saw this too often
            DEBUG_PRINT(F("ML: "));
            DEBUG_PRINTLN(F("Decode failed"));
            return false;
        }
    }

    // Success
    decodedIRData.numberOfBits = tBitIndex; // must be ML_BITS

    LongUnion tValue;
    tValue.ULong = tDecodedRawData;
    decodedIRData.decodedRawData = tDecodedRawData;
    decodedIRData.command = tValue.UByte.LowByte;
    decodedIRData.address = tValue.UByte.MidHighByte;

    // check for repeat
    if (tValue.UByte.MidLowByte & 0x01) {
        decodedIRData.flags |= IRDATA_FLAGS_IS_REPEAT;
    }
	
    decodedIRData.protocol = ML;
    return true;
}


/** @}*/
