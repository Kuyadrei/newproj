#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#include <Wire.h>
#include <RotaryEncoder.h>
#include <Button.h>


/*************************************************************************
 PiCorePlayer volume control onkyo [http]
 Nik Reitmann
 04.11.2023
 Version: 2.1

 # CHANGELOG 2.0
 - parse onkyo http get request to variable
 - send volume up/down to openhab via http rest
 - display volume 
 - added rescheduling for display timeout
 - parse harmony activity http get request to variable
 - switch for show logo instead of blank screen
 - added start "Musik" rest when knoob pressed
 - added variable for flip display rotation
 - added http get for kitchentimer display
 - created a function for display splashscreen
 - added long press knoob funktion (harmony -> powerOff)

 # CHANGELOG 2.1
 - added http timeout
 - remove rotatation timeout
 - remove wl connected contidion in main loop
 

 # INFO
## displaystate
0 = off
1 = onkyo volume
2 = kitchentimer
3 = splashscreen

***************************************************************************/


//__________________________________________CONFIG_BEGIN____________________________________________________

/// pin definitions
//rotary encoder
#define PIN_IN1               D5
#define PIN_IN2               D6
Button  knoob_button          (D7);
const int KNOOB_BUTTON_PIN =  D7;


// long press time [ms]
const int KNOOB_LONG_PRESS_TIME  = 1200;

// set WiFi credentials
#define ssid                  "YOUR-SSID"
#define password              "YOUR-WIFI-PW"

// WiFi connect timeout [ms]
const uint32_t connectTimeoutMs = 5000;

// HTTP timeout [ms]
unsigned long http_timeout      = 500;           

/// URLs FOR HTTP CLIENT
String serverName = "http://192.168.1.10:8080/"; 

//HTTP GET
String http_vol_get = "rest/items/onkyo_Volume/state";
String http_activity_get = "rest/items/harmony_wohnzimmer_activity/state";
String http_kitchentimer_get = "rest/items/timer_remain/state";
// HTTP POST
String http_activity_control_musik = "basicui/CMD?harmony_wohnzimmer_activity=Musik";
String http_activity_control_poweroff = "basicui/CMD?harmony_wohnzimmer_activity=PowerOff";
String http_vol_plus = "basicui/CMD?picore_onkyo_vol_plus=ON";
String http_vol_minus = "basicui/CMD?picore_onkyo_vol_minus=ON";

// ROTARY ENCODER DELAY
unsigned long knoob_delay = 0;           //ROTATION DELAY

///DISPLAY
// set screen resolution
#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         64

// display rotation 0=0° / 1=90° / 2=180° / 3=270°
int display_rotation = 2;

// display power timeout [ms]
unsigned long display_timeout = 4000;

// kitchen timer threshold < [min]
int kitchen_timer_threshold = 3;

//__________________________________________CONFIG_END______________________________________________________

//STRINGS
String onkyo_volume_current_http;
String harmony_activity;
String kitchentimer_remain_http;

//STATES
int kitchentimer_previous = 0;
int kitchentimer_remain = 0;
int harmony_music = 0;
int displaystate = 0;
int onkyo_volume_previous;
int onkyo_volume_current;


//TIMER
unsigned long lastTime_knoob_left = 0;
unsigned long lastTime_knoob_right = 0;
unsigned long lastTime_knoob_encoder = 0;
long display_timer = 0;

//LONG PRESS KNOOB
int knoob_lastState = LOW;
int knoob_currentState;
unsigned long knoob_pressedTime  = 0;
bool knoob_isPressing = false;
bool knoob_isLongDetected = false;

RotaryEncoder *encoder = nullptr;

//WIFI
ESP8266WiFiMulti WiFiMulti;

//DISPLAY
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// splashscreen data [TechNIK logo]
static const uint8_t TechNIK_logo[1024] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 
    0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 
    0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 
    0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
    0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 
    0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 
    0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x18, 0x03, 0x87, 0x0f, 0x03, 0xc0, 0x1e, 
    0x78, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x1c, 0x03, 0x87, 0x0f, 0x07, 0x80, 0x1e, 
    0x78, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x1e, 0x03, 0x87, 0x0f, 0x0f, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x1f, 0x03, 0x87, 0x0f, 0x0f, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x1f, 0x03, 0x87, 0x0f, 0x1e, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x1e, 0x00, 0x3e, 0x1e, 0xf8, 0x1f, 0x83, 0x87, 0x0f, 0x3c, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x7f, 0x80, 0xff, 0x9f, 0xfc, 0x1f, 0xc3, 0x87, 0x0f, 0x3c, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0xff, 0xc1, 0xff, 0x9f, 0xfe, 0x1f, 0xc3, 0x87, 0x0f, 0x78, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x01, 0xf3, 0xe3, 0xe3, 0x1f, 0x9e, 0x1f, 0xe3, 0x87, 0x0f, 0xf0, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x01, 0xe1, 0xe3, 0xc0, 0x1e, 0x0f, 0x1f, 0xf3, 0x87, 0x0f, 0xf0, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x03, 0xff, 0xf7, 0x80, 0x1e, 0x0f, 0x1e, 0x7f, 0x87, 0x0f, 0xfc, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x03, 0xff, 0xf7, 0x80, 0x1e, 0x0f, 0x1e, 0x3f, 0x87, 0x0f, 0xfc, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x03, 0xff, 0xe7, 0x80, 0x1e, 0x0f, 0x1e, 0x1f, 0x87, 0x0f, 0x9e, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x03, 0xc0, 0x07, 0x80, 0x1e, 0x0f, 0x1e, 0x1f, 0x87, 0x0f, 0x1e, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x03, 0xc0, 0x07, 0x80, 0x1e, 0x0f, 0x1e, 0x0f, 0x87, 0x0f, 0x0f, 0x00, 0x1e, 
    0x78, 0x00, 0x1e, 0x01, 0xe0, 0x03, 0xc0, 0x1e, 0x0f, 0x1e, 0x07, 0x87, 0x0f, 0x0f, 0x80, 0x1e, 
    0x78, 0x00, 0x1e, 0x01, 0xf1, 0x83, 0xe3, 0x1e, 0x0f, 0x1e, 0x07, 0x87, 0x0f, 0x07, 0x80, 0x1e, 
    0x78, 0x00, 0x1e, 0x01, 0xff, 0xc3, 0xff, 0x9e, 0x0f, 0x1e, 0x03, 0x87, 0x0f, 0x07, 0xc0, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x7f, 0xc0, 0xff, 0x9e, 0x0f, 0x1e, 0x01, 0x87, 0x0f, 0x03, 0xc0, 0x1e, 
    0x78, 0x00, 0x1e, 0x00, 0x3f, 0x00, 0x7e, 0x0e, 0x07, 0x1e, 0x01, 0x87, 0x0f, 0x01, 0xe0, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
    0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 
    0x7f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfe, 
    0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
    0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 
    0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 
    0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

IRAM_ATTR void checkPosition()
{
  encoder->tick();
}

//___________________________________SETUP_LOOP__________________________________________

void setup() {

  // setup the rotary encoder functionality
  encoder = new RotaryEncoder(PIN_IN1, PIN_IN2, RotaryEncoder::LatchMode::FOUR3);

  //knoob button begin
  knoob_button.begin();

  // register interrupt routine
  attachInterrupt(digitalPinToInterrupt(PIN_IN1), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IN2), checkPosition, CHANGE);

  // open serial connection
  Serial.begin(9600);
  delay(1000);
  Serial.println("");
  Serial.println("serial connection is ready");

  // check if display is connected
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
 
  // clear display
  display.clearDisplay();
  display.setRotation(display_rotation);

  // draw splashscreen on the screen
  display.clearDisplay();
  Serial.println("display: splashscreen"); 
  display.drawBitmap(0, 0, TechNIK_logo, 128, 64, 1);
  display.display();
  displaystate = 3;
 
  // wifi setup
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    Serial.print("wifi connected to: "); 
    Serial.println(WiFi.SSID());
    Serial.print("IP address is: ");
    Serial.println(WiFi.localIP());
  }

}



//___________________________________HTTP_POST_FUNCTIONS__________________________________________

// POWER OFF
void http_post_power_off(){

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    // do http POST
    String poweroff_harmony_string = serverName + http_activity_control_poweroff;
    http.setTimeout(http_timeout);
    http.begin(client, poweroff_harmony_string);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    http.end();
  }
}

// MUSIC ON
void http_post_music_on(){

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    // do http POST
    String start_music_string = serverName + http_activity_control_musik;
    http.setTimeout(http_timeout);
    http.begin(client, start_music_string);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    http.end();
  }
}

// VOLUME UP
void http_post_vol_up(){

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    // do http POST
    String vol_plus_string = serverName + http_vol_plus;
    http.begin(client, vol_plus_string);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    http.end();
  }
}

// VOLUME DOWN
void http_post_vol_down(){

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    // do http POST
    String vol_minus_string = serverName + http_vol_minus;
    http.setTimeout(http_timeout);
    http.begin(client, vol_minus_string);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    http.end();
  }
}

//___________________________________HTTP_GET_FUNCTIONS___________________________________________

// ONKYO VOLUME
void http_get_onkyo_volume(){ // --> RESULT = onkyo_volume_current value

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(http_timeout);

    if (http.begin(client, serverName + http_vol_get)) {  // HTTP

      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        
        // HTTP header has been send and Server response header has been handled
        String onkyo_volume_current_http = http.getString();
        onkyo_volume_current_http.trim();
        onkyo_volume_current = onkyo_volume_current_http.toInt();

      }else{

        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();

    }else{

      Serial.println("[HTTP] Unable to connect");
    }
  }
}

// HARMONY ACTIVITY
void http_get_harmony_activity(){ // --> RESULT = harmony_music = 1 or 0

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(http_timeout);

    if (http.begin(client, serverName + http_activity_get)) {  // HTTP

      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        
        // HTTP header has been send and Server response header has been handled
        String harmony_activity = http.getString();
        harmony_activity.trim();
        // string to int variable
        if (harmony_activity == "Musik"){

          harmony_music = 1;

        } else {

          harmony_music = 0;
        }

      }else{

        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();

    }else{

      Serial.println("[HTTP] Unable to connect");
    }
  }
}

// KITCHENTIMER
void http_get_kitchentimer(){ // --> RESULT = kitchentimer_remain value

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(http_timeout);

    if (http.begin(client, serverName + http_kitchentimer_get)) {  // HTTP

      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        
        // HTTP header has been send and Server response header has been handled
        String kitchentimer_remain_http = http.getString();
        kitchentimer_remain_http.trim();
        kitchentimer_remain = kitchentimer_remain_http.toInt();

      }else{

        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();

    }else{

      Serial.println("[HTTP] Unable to connect");
    }
  }
}


//___________________________________DISPLAY_FUNCTIONS___________________________________________

void display_splashscreen() {
  display.clearDisplay();
  Serial.println("display: splashscreen"); 
  display.drawBitmap(0, 0, TechNIK_logo, 128, 64, 1);
  display.display();
}

void display_volume() {
  Serial.print("draw: ");
  Serial.print(onkyo_volume_current);
  Serial.println(" to display");
  //draw to display
  display.clearDisplay();
  display.setFont(&FreeSansBold18pt7b);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(24, 55); // left/right, up/down
  display.print(onkyo_volume_current);
  display.display();
}

void display_kitchentimer(){
  Serial.println("kitchen_timer_threshold reached");
  Serial.print("draw: ");
  Serial.print(kitchentimer_remain);
  Serial.println(" min. to display");
  //draw first line to display
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(13, 12); // left/right, up/down
  display.print("Timer noch:");
  //draw second line to display
  display.setFont(&FreeSansBold12pt7b);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 56); // left/right, up/down
  display.print(kitchentimer_remain);
  display.println(" min.");
  display.display();
}

void display_clear(){
  display.clearDisplay();
  display.display();
}


//___________________________________MAIN_LOOP__________________________________________


void loop() {

  //___________________________________HTTP GET  
  http_get_onkyo_volume();
  http_get_harmony_activity();
  http_get_kitchentimer();

  //___________________________________ROTARY AND BUTTONS 

  // volume control by rotary encoder
  encoder->tick();
  int dir = (int)encoder->getDirection();
  lastTime_knoob_encoder = millis();

  if(dir != 0){

    if(dir == 1){

      if ((millis() - lastTime_knoob_right) > knoob_delay) {

        Serial.println("knoob rotate right");
        http_post_vol_up();
      }
      lastTime_knoob_right = millis();
    }

    else if(dir == -1){

      if ((millis() - lastTime_knoob_left) > knoob_delay) {

        Serial.println("knoob rotate left");
        http_post_vol_down();
      }
      lastTime_knoob_left = millis();
    }
  }

  // harmony activity "Musik" ON by knoob pressed short
  if (knoob_button.pressed()){

    Serial.println("knoob: short press");

    if(harmony_music == 0){

      Serial.println("start harmony activity music");
      http_post_music_on();
    }
  }

  // harmony "poweroff" by knoob pressed long
  knoob_currentState = digitalRead(KNOOB_BUTTON_PIN);

  if(knoob_lastState == HIGH && knoob_currentState == LOW) {

    knoob_pressedTime = millis();
    knoob_isPressing = true;
    knoob_isLongDetected = false;
  } 
  
  else if(knoob_lastState == LOW && knoob_currentState == HIGH) {

    knoob_isPressing = false;
  }

  if(knoob_isPressing == true && knoob_isLongDetected == false) {

    long pressDuration = millis() - knoob_pressedTime;

    if( pressDuration > KNOOB_LONG_PRESS_TIME ) {
      Serial.println("knoob: long press");
      knoob_isLongDetected = true;
      Serial.println("harmony: poweroff");
      http_post_power_off();
    }
  }
  knoob_lastState = knoob_currentState;

  //___________________________________DISPLAY FUNCTIONS

  // print onkyo volume to display if changed
  http_get_onkyo_volume();
  if(onkyo_volume_previous != onkyo_volume_current && displaystate != 2){

    onkyo_volume_previous = onkyo_volume_current;

    if(harmony_music == 1){

      display_volume();
      displaystate = 1;

      // reset display timer
      display_timer = millis();
      
    }else{

      if(displaystate != 2){

        displaystate = 0;
      }
    }
  }

  // print kitchentimer remain to display if changed
  if(kitchentimer_previous != kitchentimer_remain){

    kitchentimer_previous = kitchentimer_remain;
  
    if (kitchentimer_remain <= kitchen_timer_threshold) {

      if(kitchentimer_remain != 0){

        display_kitchentimer();
        displaystate = 2;
        // reset display timer
        display_timer = millis();

      } else {

        if(harmony_music == 0){

          display_clear();
          displaystate = 0;

        }else{

          display_splashscreen();
          displaystate = 3;
        }
      }
    }
  }

  //___________________________________DISPLAY CONTROLLER
/*
  ## displaystate
  0 = off
  1 = onkyo volume
  2 = kitchentimer
  3 = splashscreen
*/
  // display timer -> show logo or clear
  if(displaystate != 0 && displaystate != 2){

    if(harmony_music == 1){

      if (millis() > display_timeout + display_timer && displaystate != 3){ 

        // reset timer
        display_timer = millis();
        display_splashscreen();
        displaystate = 3;
      }

    }else{

      if(displaystate == 3){

        //reset timer
        display_timer = millis();
        display_clear();
        displaystate = 0;
      }
    }

  }else{

    if(displaystate != 2){

      //reset timer
      display_timer = millis();
      display_clear();
      displaystate = 0;
    }
  }

  // show logo at music start
  if(harmony_music == 1 && displaystate == 0 && displaystate != 3){

    //reset timer
    display_timer = millis();
    // draw splashscreen on the screen
    display_splashscreen();
    displaystate = 3;
  }

  // clear display if power off while volume is printed
  if(harmony_music == 0 && displaystate == 1){

    //reset timer
    display_timer = millis();
    display_clear();
    displaystate = 0;
  }
}
