#include <Wire.h>
#include <MsTimer2.h>
#include <SerialCommand.h>

volatile char num_signal_pattern[11][2] = {
  {0b00000000, 0b10000000}, // 0
  {0b00000100, 0b00000000}, // 1
  {0b00001000, 0b00000000}, // 2
  {0b00000000, 0b00000001}, // 3
  {0b00000000, 0b00000010}, // 4
  {0b00000000, 0b00000100}, // 5
  {0b00000000, 0b00001000}, // 6
  {0b00000000, 0b00010000}, // 7
  {0b00000000, 0b00100000}, // 8
  {0b00000000, 0b01000000}, // 9
  {0b00000000, 0b00000000}, // _
};

volatile char dot_signal_pattern[4][2] = {
  {0b00000000, 0b00000000}, // 00
  {0b00000001, 0b00000000}, // 01
  {0b00000010, 0b00000000}, // 10
  {0b00000011, 0b00000000}, // 11
};



int cycleNum = 0;
volatile char display_pattern[8][2] = {
  {0b00000000, 0b10000000}, // nix0
  {0b00000000, 0b01000000}, // nix1
  {0b00000001, 0b00000000}, // nix2
  {0b00000000, 0b00000100}, // nix3
  {0b00000000, 0b00010000}, // nix4
  {0b00000001, 0b00000000}, // nix5
  {0b00000000, 0b10000000}, // nix6
  {0b00000000, 0b10000000}, // nix7
};

volatile bool anode_signal_pattern[8][8] = {
  {1,1,1,1,1,1,1,0},
  {1,1,1,1,1,1,0,1},
  {1,1,1,1,1,0,1,1},
  {1,1,1,1,0,1,1,1},
  {1,1,1,0,1,1,1,1},
  {1,1,0,1,1,1,1,1},
  {1,0,1,1,1,1,1,1},
  {0,1,1,1,1,1,1,1},
};

volatile char cycle = 0;
volatile char before_cycle = 0;


void timer_interrupt(){
  // anode select counter
  cycle++;
  if(cycle > 7){
    cycle = 0;
  }

  digitalWrite(before_cycle+2,1);
  
  shiftOut(10,11,MSBFIRST, display_pattern[cycle][0]);
  shiftOut(10,11,MSBFIRST, display_pattern[cycle][1]);

  digitalWrite(12,LOW);
  digitalWrite(12,HIGH);
  digitalWrite(12,LOW);

  // force
  /*
  digitalWrite(2,anode_signal_pattern[cycle][0]);
  digitalWrite(3,anode_signal_pattern[cycle][1]);
  digitalWrite(4,anode_signal_pattern[cycle][2]);
  digitalWrite(5,anode_signal_pattern[cycle][3]);
  digitalWrite(6,anode_signal_pattern[cycle][4]);
  digitalWrite(7,anode_signal_pattern[cycle][5]);
  digitalWrite(8,anode_signal_pattern[cycle][6]);
  digitalWrite(9,anode_signal_pattern[cycle][7]);
  */
  
  digitalWrite(cycle+2,0);

  before_cycle = cycle;
}

// serial command control object
SerialCommand SCmd;

void setup(){
  Wire.begin();

  Serial.begin(9600);  

  // serial command
  uint8_t steps = 0;
  uint8_t addr = 0x2F;
  
  Wire.beginTransmission(addr);
  Wire.write(steps);
  Wire.endTransmission();

  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  pinMode(4,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);
  pinMode(7,OUTPUT);
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);

  // mode
  pinMode(13,OUTPUT);
  digitalWrite(13, HIGH);

  SCmd.addCommand("num", set_num);
  SCmd.addCommand("dot", set_dot);
  SCmd.addCommand("dcdc_on", dcdc_on);
  SCmd.addCommand("dcdc_off", dcdc_off);
  SCmd.addDefaultHandler(error);


  MsTimer2::set(0.1,timer_interrupt);
  MsTimer2::start();
}

int translate_num(char input_char){
  switch(input_char){
    case '0':
      return 0;
      break;

    case '1':
      return 1;
      break;

    case '2':
      return 2;
      break;

    case '3':
      return 3;
      break;

    case '4':
      return 4;
      break;

    case '5':
      return 5;
      break;
      
    case '6':
      return 6;
      break;

    case '7':
      return 7;
      break;

    case '8':
      return 8;
      break;

    case '9':
      return 9;
      break;

    case 'n':
      return 10;
      break;
  }
}



void set_num(){
  char dot_save[8] = {
    0, // nix0
    0, // nix1
    0, // nix2
    0, // nix3
    0, // nix4
    0, // nix5
    0, // nix6
    0, // nix7
  };
  
  char* arg = SCmd.next();

  //save dot
  for(char n = 0; n < 8; n++){
    dot_save[n] = (display_pattern[n][0] & 0b00000011);
  }

  memcpy(display_pattern[0], (void*)num_signal_pattern[translate_num(arg[0])], 2);
  memcpy(display_pattern[1], (void*)num_signal_pattern[translate_num(arg[1])], 2);
  memcpy(display_pattern[2], (void*)num_signal_pattern[translate_num(arg[2])], 2);
  memcpy(display_pattern[3], (void*)num_signal_pattern[translate_num(arg[3])], 2);
  memcpy(display_pattern[4], (void*)num_signal_pattern[translate_num(arg[4])], 2);
  memcpy(display_pattern[5], (void*)num_signal_pattern[translate_num(arg[5])], 2);
  memcpy(display_pattern[6], (void*)num_signal_pattern[translate_num(arg[6])], 2);
  memcpy(display_pattern[7], (void*)num_signal_pattern[translate_num(arg[7])], 2);

  //recover dot
  for(char n = 0; n < 8; n++){
    display_pattern[n][0] = (display_pattern[n][0] | dot_save[n]);
  }
}

void set_dot(){
  char* arg = SCmd.next();

  display_pattern[0][0] = ((display_pattern[0][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[0])][0]);
  display_pattern[1][0] = ((display_pattern[1][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[1])][0]);
  display_pattern[2][0] = ((display_pattern[2][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[2])][0]);
  display_pattern[3][0] = ((display_pattern[3][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[3])][0]);
  display_pattern[4][0] = ((display_pattern[4][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[4])][0]);
  display_pattern[5][0] = ((display_pattern[5][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[5])][0]);
  display_pattern[6][0] = ((display_pattern[6][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[6])][0]);
  display_pattern[7][0] = ((display_pattern[7][0] & 0b11111100) | dot_signal_pattern[translate_num(arg[7])][0]);
}

void dcdc_on(){
  digitalWrite(13, HIGH);
}

void dcdc_off(){
  digitalWrite(13, LOW);
}

void error(){
  Serial.println("ubnrecongized command");
}

void loop(){
  SCmd.readSerial();
}
