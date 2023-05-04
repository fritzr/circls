#include <avr/power.h>
#define EXCLUDE_UNIVERSAL_PROTOCOLS enabled
#include <IRremote.h>
#include "RS-FEC.h"

#define WIDTH 150

// enable IR
IRrecv irrecv(3);

// common cathode values
uint8_t symbols[] = {
    0b0010, // red   (PIN 9)
    0b0100, // green (PIN 10)
    0b1000, // blue  (PIN 11)
    0b0110  // yellow
  };

char packet[] =
    "\x0"          // id
    "Hello world!" // message
    "\x0\x0\x0";   // parity
/*
 *    11  00 01 00 01  GRGR
 * H  48  01 00 10 00  RBRG
 * e  65  01 10 01 01  GGBG
 * l  6c  01 10 11 00  RYBG
 * l  6c  01 10 11 00  RYBG
 * o  6f  01 10 11 11  YYBG
 *    20  00 10 00 00  RRBR
 * w  77  01 11 01 11  YGYG
 * o  6f  01 10 11 11  YYBG
 * r  72  01 11 00 10  BRYG
 * l  6c  01 10 11 00  RYBG
 * d  64  01 10 01 00  RGBG
 * !  21  00 10 00 01  GRBR
 *    bb  10 11 10 11  YBYB
 *    dc  11 01 11 00  RYGY
 *    1d  00 01 11 01  GYGR
 *    5b  01 01 10 11  YBGG
 */

#define NMSG 12
#define NPAR 4
RS::ReedSolomon<NMSG, NPAR> rs;

void setup() {
  // configure IR pins
  irrecv.enableIRIn();

  // configure LED pins
  DDRB |= 0b1110;

  // set packet data length
  packet[0] = NMSG + NPAR + 1;

  // calculate parity
  rs.Encode((packet + 1), (packet + 1));

  Serial.begin(115200);
  for (int i = 0; i < sizeof(packet); i++) {
    Serial.print((byte)packet[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
//  Serial.end();
}

void loop() {
  // send on/off symbols for synchronization
  for (uint8_t i = 0; i < 4; i++) {
    PORTB = 0b0110;
    delayMicroseconds(WIDTH);
    PORTB = 0b0000;
    delayMicroseconds(WIDTH*3);
  }

  // send message
  for (uint16_t i = 0; i < sizeof(packet); i++) {
    for (uint8_t j = 0; j < 8; j += 2) {
      uint8_t k = (packet[i] >> j) & 0b11;
      PORTB = symbols[k];
      delayMicroseconds(WIDTH);
      PORTB = 0b0000;
      delayMicroseconds(WIDTH);
    }
  }

  // use IR data to reset packet # if necessary
  int16_t data = decodeIR();
  if (data >= 0) {
    Serial.println(data);
    packet[0] = data;
  } else {
    packet[0]++;
  }
}

int16_t decodeIR()
{
  if (irparams.StateForISR != IR_REC_STATE_STOP) return -1;

  uint16_t ret = 0;
  for (uint8_t i = 1; i < 32; i += 2) {
    ret <<= 1;
    bool b = irparams.rawbuf[i] > irparams.rawbuf[i+1] ? 1 : 0;
    ret |= b;
  }
  irrecv.resume();

  // check magic number
  if (ret >> 8 != 0b10100000) return -1;

  return ret & 0xFF;
}
