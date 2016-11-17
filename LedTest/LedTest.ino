#define RED   10
#define GREEN 9
#define BLUE  11
#define ANODE 12
//define CATHODE

#define IR_OUT 3
#define IR_GND 4
#define IR_VCC 5

uint32_t pattern[] = {
    0x000000, // off
    0xff0000, // red
    0xff2000, // orange
    0xff9000, // yellow
    0x00ff00, // green
    0x0000ff, // blue
    0x500050, // purple
    0xffffff  // white
  };

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

  Serial.begin(115200);
}

void loop() {
  for (uint16_t i = 0; i < sizeof(pattern) / sizeof(long); i++) {
    setColor(pattern[i]);
    delay(1000);
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

