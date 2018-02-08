#include <avr/power.h>
extern "C"
{
    #include "ecc.h"
}

#define WIDTH 200

// common cathode values
uint8_t symbols[] = {
    0b0010, // red   (PIN 9)
    0b0100, // green (PIN 10)
    0b1000, // blue  (PIN 11)
    0b0110, // yellow
  };

char packet[] =
    "\x0"          // length
    "Hello world!" // message
    "\x0\x0\x0";   // parity
/*
 *    10  00 01 00 00  RRGR
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
 *    dc  11 01 11 00  RYGY
 *    83  10 00 00 11  YRRB
 *    d5  11 01 01 01  GGGY
 *    c3  11 00 00 11  YRRY
 */

void encode_rs (uint8_t *__restrict outbuf, uint8_t *inbuf, size_t insize)
{
  initialize_ecc ();

  size_t in_idx = 0;
  size_t out_idx = 0;
  size_t left = insize;

  /* Encode data in 255-byte chunks, where NPAR of the bytes are for parity.  */
  while (left > 0)
  {
    size_t in_chunk = 255 - NPAR;
    if (left < in_chunk)
      in_chunk = left;

    encode_data (inbuf + in_idx, in_chunk, outbuf + out_idx);

    in_idx += in_chunk;
    out_idx += in_chunk + NPAR;
    left -= in_chunk;
  }
}

void setup() {
  // shutdown everything unnecessary
  power_adc_disable();
  power_twi_disable();
  power_usart0_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();

  // configure output pins
  DDRB |= 0b1110;

  // set packet length
  packet[0] = strlen(packet + 1) + NPAR;

  // calculate parity
  encode_rs((uint8_t *) &packet[1], (uint8_t *) &packet[1], strlen(packet + 1));
}

void loop() {
  // send on/off symbols for synchronization
  for (uint8_t i = 0; i < 4; i++) {
    PORTB = 0b0110;
    delayMicroseconds(WIDTH);
    PORTB = 0b0000;
    delayMicroseconds(WIDTH*2);
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
}

