// Copyright 2014-2015 the project authors as listed in the AUTHORS file.
// All rights reserved. Use of this source code is governed by the
// license that can be found in the LICENSE file.

#include "ArduinoDS18B20Sensor.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEVICE_ID 8
#define DEVICE_NAME "ArduinoDS18B20Sensor"

#define MIN_PULSE_LENGTH 100
#define SYNC_DURATION_MIN 700
#define SYNC_DURATION_MAX 1200
#define EXPECTED_MESSAGE_BITS 32
#define ONE_PULSE_MIN_LENGTH 450

ArduinoDS18B20Sensor::ArduinoDS18B20Sensor() : Device() {
   syncCount = 0;
   syncFound = false;
   bitCount = 0;
   code = 0;
   this->houseCode = houseCode; 
   memset(durations,0,sizeof(int)*BITS_IN_MESSAGE);
   pulseCount = 0;
}

int ArduinoDS18B20Sensor::deviceType(void) {
   return DEVICE_ID;
};

char* ArduinoDS18B20Sensor::deviceName(void) {
   return (char*) DEVICE_NAME;
};

void ArduinoDS18B20Sensor::processPulse(long duration) {
   // any pulse less than MIN_PULSE_LENGTH means we have noise so we are not in the middle
   // of a transmision
   if (duration < MIN_PULSE_LENGTH) {
      syncFound = false;
      syncCount = 0;
      return;
   }

   // capture the next bit and decode if we are at the end of the transmission
   if (syncFound) {
      pulseCount++;
      if (0 != (pulseCount%2)) { 
	 // we only look at every other pulse which is the one at the high level 
         return;
      }

      // we are processing a message record the duration of the pulse which 
      // we will use to build the overall message 
      durations[bitCount] = duration;
      bitCount++;
 
      // ok we have all 32 bits that we expect 
      if (BITS_IN_MESSAGE == bitCount) {
         for (int i=0; i < BITS_IN_MESSAGE; i++) {
             code = code << 1;
             if (ONE_PULSE_MIN_LENGTH > durations[i]) {
                code = code | 0x1;
             }
         }

         if (validateChecksum(code)) { 
            Message* newMessage = queue->getFreeMessage();
            if (NULL != newMessage) {
		memset(newMessage, 0, sizeof(Message));
                newMessage->device = (void*) this;
                newMessage->timestamp = time(NULL); 
                newMessage->code = code;
                queue->enqueueMessage(newMessage);
            } else {
               // no messages available so just drop this value
            }
         } 

        // ok wait for the next message
        syncFound = false;
      } 
   } else {
      if ((duration > SYNC_DURATION_MIN) && (duration < SYNC_DURATION_MAX)) {
         syncCount++;
         if (syncCount == 10 ) {
            code = 0;
            syncFound = true;
            syncCount = 0;
            bitCount = 0;
            pulseCount = 0;
         }
      } else {
         syncCount = 0;
      }
   }
};

void ArduinoDS18B20Sensor::decodeMessage(Message* message) {
   char* bytes = (char*) &message->code;
   float temp = (bytes[2] * 256 + bytes[1]);
   temp = temp/10 - 128;
   message->type = 1;
   sprintf(message->text, "%ld, %x - temp: %f",
           message->timestamp,
           message->code,
           temp);
};

void ArduinoDS18B20Sensor::publishTopic(Message* message, char* buffer, int maxLength) {
   // will not be called but still has to be implemented
   // until we remove and update the rest of the devices
}

int ArduinoDS18B20Sensor::numMessages(void) {
  return 1;
}

void ArduinoDS18B20Sensor::publishTopic(int messageNum, Message* message, char* buffer, int maxLength) {
   char* bytes = (char*) &message->code;
   snprintf(buffer, maxLength, "house/arduinoDS18B20Sensor/%x/temp", bytes[3]);
}

void ArduinoDS18B20Sensor::getMessageText(int messageNum, Message* message, char* buffer, int maxLength) {
   char* bytes = (char*) &message->code;
   float temp = ((float)(bytes[2] * 256 + bytes[1]))/10 - 128;
   sprintf(buffer, "%ld, %x - temp: %f",
           message->timestamp,
           message->code,
           temp);
}

// to calculate the checksum first reverse the nibbles in each byte
// including the checksum 
// then add the 3 message bytes together and add 0x77 to get the 
// expected checksum which is in byte 0 
bool ArduinoDS18B20Sensor::validateChecksum(int code) {
   char* bytes = (char*) & code;
   char calcChecksum = 0x88;
   char checksum = ((bytes[0] >> 4) & 0x0F) + ((bytes[0] & 0x0F) << 4);

   for (int i = 1;i<4;i++) {
      calcChecksum += ((bytes[i] >> 4) & 0x0F) + ((bytes[i] & 0x0F) << 4);
   }

   if (calcChecksum == checksum) {
      return true;
   }

   return false;
}

