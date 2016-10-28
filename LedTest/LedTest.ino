#define RED   10
#define GREEN 11
#define BLUE  9
#define ANODE 8
//define CATHODE

uint32_t pattern[] = { 
    0x000000, // off
    0xff0000, // red
    0xffa500, // orange
    0xffff00, // yellow
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
}

void loop() {
  for (uint16_t i = 0; i < sizeof(pattern) / sizeof(long); i++) {
    setColor(pattern[i]);
    delay(1000);
  }
}

void setColor(uint32_t rgb) {
  uint8_t red = (rgb >> 16) & 0xff;
  uint8_t green = (rgb >> 8) & 0xff;
  uint8_t blue = rgb & 0xff;

  #ifdef ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  #endif

  analogWrite(RED, red);
  analogWrite(GREEN, green);
  analogWrite(BLUE, blue);  
}

