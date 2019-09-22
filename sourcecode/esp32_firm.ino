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
unsigned int dpmode = 0;

AsyncWebServer server(80);

const int SW3 = 14;
const int SW4 = 27;
const int SW5 = 26;

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
  5,
  6,
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


String html ="<!DOCTYPE html> <html lang=\"ja\"> <head> <meta charset=\"utf-8\"> <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <title>Bootstrap Sample</title> <link href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" rel=\"stylesheet\"> <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script> <script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.0/umd/popper.min.js\"></script> <script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\"></script> </head> <body> <header style=\"background-color:white\"></header> <form> <div class=\"container-fluid\"> <div class=\"row\"> <div class=\"container\"> <h3>ボタンのモード設定</h3> <table class=\"table table-bordered\"> <thead> <tr> <th>ボタンの名前</th> <th>モード</th> </tr> </thead> <tbody> <tr> <th scope=\"row\">func1</th> <td> <select name=\"func1\"> <option value=\"0\">日付</option> <option value=\"1\">時刻</option> <option value=\"2\">気圧</option> <option value=\"3\">気温、湿度</option> <option value=\"4\">緯度経度</option> <option value=\"5\">APIモード</option> <option value=\"6\">タイマー</option> </select> </td> </tr> <tr> <th scope=\"row\">func2</th> <td> <select name=\"func2\"> <option value=\"0\">日付</option> <option value=\"1\">時刻</option> <option value=\"2\">気圧</option> <option value=\"3\">気温、湿度</option> <option value=\"4\">緯度経度</option> <option value=\"5\">APIモード</option> <option value=\"6\">タイマー</option> </select> </td> </tr> </tbody> </table> </div> <br> <div class=\"container\"><br> <h3>Wifi設定</h3> <p>現在のIPアドレスは</p> <!-- Wifi情報をとってくる --> <p>現在接続中のWifiは</p> <!-- Wifi情報をとってくる --> <br> <div class=\"form-group\"> <h3>NTPアドレス</h3> <input type=\"email\" class=\"form-control\" id=\"exampleInputaddr\" placeholder=\"example\"> </div> </form> <br> <div class=\"checkbox\"> <h3>有効にするモード</h3> <label> <input type=\"checkbox\" name=\"date\"> 日付<br> <input type=\"checkbox\" name=\"clock\"> 時刻<br> <input type=\"checkbox\" name=\"pressure\"> 気圧<br> <input type=\"checkbox\" name=\"temp\"> 気温、湿度<br> <input type=\"checkbox\" name=\"gps\"> 緯度、経度<br> <input type=\"checkbox\" name=\"api\"> APIモード<br> <input type=\"checkbox\" name=\"timer\"> タイマー<br> </label> </div> <button type=\"submit\" class=\"btn btn-primary\">送信</button> </div> </div> </div> </form> <footer style=\"background-color:white\"></footer> </body> </html>";

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
    String temp = String(command + String(display_num[n]));
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
  WiFi.disconnect(true);
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
  serial0.println(" CONNECTED");
  
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
          if(param[i] == ':'){
            // display_pattern[i][0] = 0;
            // display_pattern[i][1] = 0;
            continue;
          }
        
          // memcpy(display_pattern[i], (void*)num_signal_pattern[param[i] - '0'], 2);
      
        }
      }

      if(p->name() == "dot" && dpmode == 4){
        String param = p->value();

        for(char i = 0; i < 8; ++i){
          // display_pattern[i][0] &= 0b11111100;

          switch(param[i] - '0'){
            case 0:
              // display_pattern[i][0] |= dot_signal_pattern[0][0];
              break;
            case 1:
              // display_pattern[i][0] |= dot_signal_pattern[1][0];
              break;
            case 2:
              // display_pattern[i][0] |= dot_signal_pattern[2][0];
              break;
            case 3:
              // display_pattern[i][0] |= dot_signal_pattern[3][0];
              break;
          }
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
  /*
  memcpy(display_pattern[0], (void*)num_signal_pattern[now.hour()%100/10], 2);
  memcpy(display_pattern[1], (void*)num_signal_pattern[now.hour()%10], 2);
  memcpy(display_pattern[2], (void*)dot_signal_pattern[2], 2);
  memcpy(display_pattern[3], (void*)num_signal_pattern[now.minute()%100/10], 2);
  memcpy(display_pattern[4], (void*)num_signal_pattern[now.minute()%10], 2);
  memcpy(display_pattern[5], (void*)dot_signal_pattern[2], 2);
  memcpy(display_pattern[6], (void*)num_signal_pattern[now.second()%100/10], 2);
  memcpy(display_pattern[7], (void*)num_signal_pattern[now.second()%10], 2);
  */
}

void setDisplayDate(DateTime now){
}

void setDisplayThermoHumidity(double temperature, double humidity){

}

void setDisplayPressure(double pressure){
}

double temp_act, press_act, hum_act; //最終的に表示される値を入れる変数

void loop() {

}
