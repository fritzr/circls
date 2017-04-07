#define RED   9
#define GREEN 10
#define BLUE  11
//define ANODE 8
#define CATHODE 8

#define IR_OUT 3
#define IR_GND 4
#define IR_VCC 5

uint32_t symbols[] = {
    0xff0000, // red
    0x00ff00, // green
    0x0000ff, // blue
    0xffffff, // white
  };

char message[] = "Hello world!";
/*
 * H  48  01 00 10 00  RBRG
 * e  65  01 10 01 01  GGBG
 * l  6c  01 10 11 00  RWBG
 * l  6c  01 10 11 00  RWBG
 * o  6f  01 10 11 11  WWBG
 *    20  00 10 00 00  RRBR
 * w  77  01 11 01 11  WGWG
 * o  6f  01 10 11 11  WWBG
 * r  72  01 11 00 10  BRWG
 * l  6c  01 10 11 00  RWBG
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
  for (uint16_t i = 0; i < strlen(message); i++) {
    for (uint8_t j = 0; j < 8; j += 2) {
      uint8_t k = (message[i] >> j) & 0b11;
      setColor(symbols[k]);
      delayMicroseconds(500);
      setColor(0);
      delayMicroseconds(500);
    }
  }
}

void setColor(uint32_t rgb) {
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

