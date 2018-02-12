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
 *    38  00 11 10 00  RBYR
 *    9d  10 01 11 01  GYGB
 *    04  00 00 01 00  RGRR
 *    1c  00 01 11 00  RYGR
 */

void setup() {
  // configure output pins
  DDRB |= 0b1110;

  // set packet length
  size_t len = strlen(packet + 1);
  packet[0] = len + NPAR;

  // calculate parity
  initialize_ecc ();
  encode_data((packet + 1), len, (packet + 1));

  Serial.begin(115200);
  for (int i = 0; i < sizeof(packet); i++) {
    Serial.print(packet[i], HEX);
    Serial.print(' ');
  }
  Serial.end();

  // shutdown everything unnecessary
  power_adc_disable();
  power_twi_disable();
  power_usart0_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
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

