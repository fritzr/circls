#define RED   9
#define GREEN 10
#define BLUE  11

//define BLUE  9
//define RED   10
//define GREEN 11

#define CATHODE 8
//define ANODE 8

#define IR_OUT 3
#define IR_GND 4
#define IR_VCC 5

#define WIDTH 500

uint8_t symbols[] = {
    0b0010, // red
    0b0100, // green
    0b1000, // blue
    0b0110, // yellow
  };

char message[] = "Hello world!";
/*
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
 */

void setup() {
  #ifdef ANODE
    pinMode(ANODE, OUTPUT);
    digitalWrite(ANODE, HIGH);
  #else
    pinMode(CATHODE, OUTPUT);
    digitalWrite(CATHODE, LOW);
  #endif

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  pinMode(IR_OUT, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_OUT), irReceive, RISING);
  
  pinMode(IR_GND, OUTPUT);
  digitalWrite(IR_GND, LOW);
  
  pinMode(IR_VCC, OUTPUT);
  digitalWrite(IR_VCC, HIGH);
}

void loop() {
  // send 4 white/off symbols for synchronization
  for (uint8_t i = 0; i < 4; i++) {
    PORTB = symbols[i];
    delayMicroseconds(WIDTH);
    PORTB = 0b0000;
    delayMicroseconds(WIDTH*2);
  }

  // message
  for (uint16_t i = 0; i < strlen(message); i++) {
    for (uint8_t j = 0; j < 8; j += 2) {
      uint8_t k = (message[i] >> j) & 0b11;
      PORTB = symbols[k];
      delayMicroseconds(WIDTH);
      PORTB = 0b0000;
      delayMicroseconds(WIDTH*2);
    }
  }
}

void setColor(uint8_t rgb) {
  #ifdef ANODE
    rgb = ~rgb;
  #endif

  analogWrite(RED, rgb >> 16);
  analogWrite(GREEN, rgb >> 8);
  analogWrite(BLUE, rgb);
}

void irReceive() {
  UDR0 = '.';
}

