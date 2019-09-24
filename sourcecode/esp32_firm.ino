#include <Time.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <Wire.h>

#include "RTClib.h"
RTC_DS3231 rtc;

#include <SSCI_BME280.h>
SSCI_BME280 bme280;
uint8_t i2c_addr = 0x77; 

#include "ESPAsyncWebServer.h"
#include <TinyGPS.h>

// Timer Interrupt setting
hw_timer_t * timer = NULL;

volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

const char* ssid = "yurucamp";
const char* pass = "mokemoke";

const int8_t timezone = 9;
unsigned int dpmode = 1;

AsyncWebServer server(80);

const int SW3 = 26;
const int SW4 = 27;
const int SW5 = 14;

volatile char func_btn[2] = {
  0,
  0,
};

// nixie tube disolay num
// 0-9:そのまま
// 10: display none
volatile int display_num[8] = {
  0,
  1,
  2,
  3,
  4,
  10,
  10,
  7,
};

// dot none:0
// only right dot:1
// only left dot:2
// right and left dot:3
volatile int display_dot[8] = {
  0,
  1,
  0,
  0,
  1,
  0,
  0,
  0,
};

volatile int display_dot_server[8] = {
  0,
  1,
  2,
  0,
  0,
  0,
  0,
  0,
};

volatile int display_num_server[8] = {
  0,
  1,
  2,
  3,
  4,
  10,
  10,
  7,
};


volatile bool func_enable[10] = {
  true,
  true,
  true,
  true,
  true,
  true,
  true,
  true,
  true,
  true,
};


String html ="<!DOCTYPE html> <html lang=\"ja\"> <head> <meta charset=\"utf-8\"> <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <title>NIXIE TUBE CONTROL</title> <link href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" rel=\"stylesheet\"> <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script> <script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.0/umd/popper.min.js\"></script> <script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\"></script> </head> <body> <header style=\"background-color:white\"></header> <form> <div class=\"container-fluid\"> <div class=\"row\"> <div class=\"container\"> <h3>ボタンのモード設定</h3> <table class=\"table table-bordered\"> <thead> <tr> <th>ボタンの名前</th> <th>モード</th> </tr> </thead> <tbody> <tr> <th scope=\"row\">func1</th> <td> <select name=\"func1\"> <option value=\"0\">日付</option> <option value=\"1\">時刻</option> <option value=\"2\">気圧</option> <option value=\"3\">気温、湿度</option><option value=\"4\">APIモード</option></select> </td> </tr> <tr> <th scope=\"row\">func2</th> <td> <select name=\"func2\"> <option value=\"0\">日付</option> <option value=\"1\">時刻</option> <option value=\"2\">気圧</option> <option value=\"3\">気温、湿度</option> <option value=\"4\">APIモード</option> </select> </td> </tr> </tbody> </table> </div> <br> <div class=\"container\"><br> <div class=\"form-group\"> <h3>NTPアドレス</h3> <input type=\"email\" class=\"form-control\" id=\"exampleInputaddr\" placeholder=\"example\"> </div> </form> <br> <div class=\"checkbox\"> <h3>有効にするモード</h3> <label> <input type=\"checkbox\" name=\"date\"> 日付<br> <input type=\"checkbox\" name=\"clock\"> 時刻<br> <input type=\"checkbox\" name=\"pressure\"> 気圧<br> <input type=\"checkbox\" name=\"temp\"> 気温、湿度<br> <input type=\"checkbox\" name=\"api\"> APIモード<br> <input type=\"checkbox\" name=\"timer\"> タイマー<br> </label> </div> <button type=\"submit\" class=\"btn btn-primary\">送信</button> </div> </div> </div> </form> <footer style=\"background-color:white\"></footer> </body> </html>";


HardwareSerial serial0(0);
HardwareSerial atmega_serial(2);

void adjustByNTP(){
  configTzTime("JST-9", "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  rtc.adjust(DateTime(time(NULL))+TimeSpan(0, timezone, 0, 0));
}

void setNum(){
  String command = "num ";

  for(int n = 0; n < 8; n++){
    String temp;

    if(display_num[n] == 10){
      temp = String(command + String("n"));
    }else{
      temp = String(command + String(display_num[n]));
    }
    
    command = temp;
  }

  // for debug
  // serial0.println(command);
  atmega_serial.println(command);
}

void setDot(){
  String command = "dot ";

  for(int n = 0; n < 8; n++){

    String temp = String(command + String(display_dot[n]));
    command = temp;
  }

  // for debug
  // serial0.println(command);
  atmega_serial.println(command);
}

void setup() {
  pinMode(4,OUTPUT);
  pinMode(19,OUTPUT);
  pinMode(18,OUTPUT);

  // speaker high
  digitalWrite(4,HIGH);

  //button setting
  pinMode(SW3,INPUT);
  pinMode(SW4,INPUT);
  pinMode(SW5,INPUT_PULLUP);

  delay(5000);

  serial0.begin(115200);

  // open arduino serial
  // rx:19 tx:18
  atmega_serial.begin(9600, SERIAL_8N1, 19, 18);

  setNum();
  setDot();
  
  /*
  // for test
  atmega_serial.println("dcdc_on");
  atmega_serial.println("num 11451419");
  */

  bool isWiFiConnected = true;

  serial0.printf("Connecting to %s ", ssid);
  // WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  unsigned long time = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    serial0.print(".");
    
    if(millis()-time>10000){
      serial0.print("Can't connect Wi-Fi");
      WiFi.disconnect(true);
      isWiFiConnected = false;
      break;
    }
  }

  // io32 led
  pinMode(32, OUTPUT);
  digitalWrite(32, isWiFiConnected);

  if(isWiFiConnected){
    serial0.println(" CONNECTED");
    serial0.println(WiFi.localIP());
  }
  
  //RTCのあれこれ
  if (! rtc.begin()) {
    serial0.println("Couldn't find RTC");
    while (1);
  }
  
  if(isWiFiConnected){
    adjustByNTP();
  }

  //bme280 set up
  uint8_t osrs_t = 1;             //Temperature oversampling x 1
  uint8_t osrs_p = 1;             //Pressure oversampling x 1
  uint8_t osrs_h = 1;             //Humidity oversampling x 1
  uint8_t bme280mode = 3;         //Normal mode
  uint8_t t_sb = 5;               //Tstandby 1000ms
  uint8_t filter = 0;             //Filter off
  uint8_t spi3w_en = 0;           //3-wire SPI Disable

  bme280.setMode(i2c_addr, osrs_t, osrs_p, osrs_h, bme280mode, t_sb, filter, spi3w_en);
  bme280.readTrim();
  
  server.on("/setting", HTTP_GET, [&](AsyncWebServerRequest *request){
 
    int paramsNr = request->params();
    serial0.println(paramsNr);
 
    for(int i=0;i<paramsNr;i++){
      AsyncWebParameter* p = request->getParam(i);

      if(p->name() == "num" && dpmode == 4){
        String param = p->value();

        for(char i = 0; i < 8; i++){
          if (param[i] == 'n'){
            display_num_server[i] = 10;
          }else{
            display_num_server[i] = param[i] - '0';
          }
        }
      }

      if(p->name() == "dot" && dpmode == 4){
        String param = p->value();

        for(char i = 0; i < 8; ++i){
          display_dot_server[i] = param[i] - '0';
        }
      }

      if(p->name() == "mode"){
        dpmode = p->value()[0] - '0';
      }
    }
 
    request->send(200, "text/html", "<p>message received</p>");
  });

  server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
    int params_num = request->params();

    bool flag = false;
    bool temp_func_enable[10] = {
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
    };

    for(int i=0; i < params_num; ++i){
      AsyncWebParameter* p = request->getParam(i);

      if(p->name() == "func1"){
        func_btn[0] = p->value()[0] - '0';
      }

      if(p->name() == "func2"){
        func_btn[1] = p->value()[0] - '0';
      }

      if(p->name() == "clock"){
        flag = true;
        temp_func_enable[0] = true;
      }

      if(p->name() == "date"){
        flag = true;
        temp_func_enable[1] = true;
      }

      if(p->name() == "temp"){
        flag = true;
        temp_func_enable[2] = true;
      }

      if(p->name() == "pressure"){
        flag = true;
        temp_func_enable[3] = true;
      }

      if(p->name() == "api"){
        flag = true;
        temp_func_enable[4] = true;
      }

      if(p->name() == "gps"){
        flag = true;
        temp_func_enable[5] = true;
      }

      if(p->name() == "timer"){
        flag = true;
        temp_func_enable[6] = true;
      }

    }

    if(flag){
      for(char i = 0; i < 10; ++i){
        func_enable[i] = temp_func_enable[i];
      }
    }

    request->send(200, "text/html", html);
  });
  
  server.begin();
}

void setDisplayTime(DateTime now){
  
  display_num[0] = now.hour()%100/10;
  display_num[1] = now.hour()%10;
  display_num[2] = 10;
  display_num[3] = now.minute()%100/10;
  display_num[4] = now.minute()%10;
  display_num[5] = 10;
  display_num[6] = now.second()%100/10;
  display_num[7] = now.second()%10;

  display_dot[0] = 0;
  display_dot[1] = 0;
  display_dot[2] = 2;
  display_dot[3] = 0;
  display_dot[4] = 0;
  display_dot[5] = 2;
  display_dot[6] = 0;
  display_dot[7] = 0;
  
}

void setDisplayDate(DateTime now){
  display_num[0] = now.year()%100/10;
  display_num[1] = now.year()%10;
  display_num[2] = 10;
  display_num[3] = now.month()%100/10;
  display_num[4] = now.month()%10;
  display_num[5] = 10;
  display_num[6] = now.day()%100/10;
  display_num[7] = now.day()%10;

  display_dot[0] = 0;
  display_dot[1] = 0;
  display_dot[2] = 2;
  display_dot[3] = 0;
  display_dot[4] = 0;
  display_dot[5] = 2;
  display_dot[6] = 0;
  display_dot[7] = 0;
}

void setDisplayThermoHumidity(double temperature, double humidity){
  display_num[0] = (int)temperature%100/10;
  display_num[1] = (int)temperature%10/1;
  display_num[2] = (int)round(temperature*10)%10;
  display_num[3] = 10;
  display_num[4] = 10;
  display_num[5] = (int)humidity%100/10;
  display_num[6] = (int)humidity%10/1;
  display_num[7] = (int)round(humidity*10)%10;

  display_dot[0] = 0;
  display_dot[1] = 2;
  display_dot[2] = 0;
  display_dot[3] = 0;
  display_dot[4] = 0;
  display_dot[5] = 0;
  display_dot[6] = 2;
  display_dot[7] = 0;
}

void setDisplayPressure(double pressure){
  display_num[0] = 10;
  display_num[1] = 10;
  display_num[2] = (int)pressure%10000/1000 == 0 ? 10 : (int)pressure%10000/1000;
  display_num[3] = (int)pressure%1000/100;
  display_num[4] = (int)pressure%100/10;
  display_num[5] = (int)pressure%10/1;
  display_num[6] = (int)(pressure*10)%10;
  display_num[7] = (int)(pressure*100)%10;

  display_dot[0] = 0;
  display_dot[1] = 0;
  display_dot[2] = 0;
  display_dot[3] = 0;
  display_dot[4] = 0;
  display_dot[5] = 2;
  display_dot[6] = 0;
  display_dot[7] = 0;
}

void setServerNumDot(){
  for(int i = 0; i < 8; i++){
    display_dot[i] = display_dot_server[i];
    display_num[i] = display_num_server[i];
  }
}

double temp_act, press_act, hum_act; //最終的に表示される値を入れる変数
DateTime now_time;

void loop() {
  if(!digitalRead(SW3)){
    while(!digitalRead(SW3));
    dpmode++;

    while(!func_enable[dpmode]){
      dpmode++;
    }

    if(dpmode>4) dpmode = 0;
  }

  if(!digitalRead(SW4)){
    while(!digitalRead(SW4));

    dpmode = func_btn[0];
  }

  if(!digitalRead(SW5)){
    while(!digitalRead(SW5));

    dpmode = func_btn[1];
  }
  
  switch (dpmode)
  {
  case 0:
    now_time = rtc.now();
    setDisplayDate(now_time);
    break;

  case 1:
    now_time = rtc.now();
    setDisplayTime(now_time);
    break;
  
  case 2:
    bme280.readData(&temp_act, &press_act, &hum_act);
    setDisplayThermoHumidity(temp_act, hum_act);
    delay(100);
    break;

  case 3:
    bme280.readData(&temp_act, &press_act, &hum_act);
    setDisplayPressure(press_act);
    delay(100);
    break;

  case 4:
    //api mode
    setServerNumDot();
    delay(10);
    break;
  }

  setNum();
  setDot();
}
