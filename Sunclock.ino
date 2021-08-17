// **********************************************************
// Sunrise Alarm Clock
//
// Author: Jordan Hay
// https://jordanhay.com
// 
// WEMOS D1 Mini based sunrise style alarm clock.
// 
// NOTE THAT THIS CODE IS SETUP TO WORK WITH WPA2 ENTERPRISE
// WIFI-CENTRIC CODE WILL NEED REPLACED FOR OTHER NETWORKS
// **********************************************************

// **********
// INCLUSIONS
// **********

#include <ESP8266WiFi.h> // ESP8226 Control
#include <NTPClient.h> // NTP for syncing time
#include <WiFiUdp.h> // Connect to NTP server
#include <TimeLib.h> // Time management
#include <Adafruit_NeoPixel.h> // Neopixels

// External C libraries for wifi connection on enterprise wifi
extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
#include "c_types.h"
}

// ******************
// GLOBAL DEFINITIONS
// ******************

// WiFi credentials
char ssid[] = "";
char username[] = "";
char identity[] = "";
char password[] = "";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "nz.pool.ntp.org");

// ESP Mac
uint8_t target_esp_mac[6] = {0x24, 0x0a, 0xc4, 0x9a, 0x58, 0x28};

// Neopixel pin
#define NP D1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, NP, NEO_GRB + NEO_KHZ800);

// Store time
float currentSecond;

// Alarm vals
float alarmStart;
float alarmEnd;

// Alarm Buffer times
float aBufferStart;
float aBufferEnd;

// Cooldown time
float cooldownStart;
float cooldownEnd;

// ********************
// FUNCTION DEFINITIONS
// ********************

// ********************
// Idle Animation
//
// Plays during the day
// Rainbow Cycle
// ********************
void idleAnimation(float endTime) {
  
  // Set low brightness
  strip.setBrightness(20);
  
  // Cycle through hues
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    
    // For each pixel
    for(int i=0; i<strip.numPixels(); i++) { 
      
      // Set to a hue value
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    
    // Update strip
    strip.show();
    // Pause for thought
    delay(100);

    // If the current time is greater than the end time
    if(currentSecond >= endTime) {
      // Get us out of this loop
      break;
    }
  }
}

// ********************************************
// Alarm Animation
//
// Plays during the alarm period in the morning
// Cycles from deep dark red to bright white
// ********************************************
void alarmAnimation() {
  
  // Calculate new value of proportion
  float prop = proportionDone(alarmStart, alarmEnd);
  // Set brightness in accordance with proportion done
  strip.setBrightness(255 * prop);
  // Update strip
  strip.show();

  // Update every pixel to match colour set
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 255*prop/3, 255*prop/5 + 255/5));
    strip.show();
  }
}

// *******************************************************
// Cooldown Animation
//
// Plays at night time leading up to bed time
// Fades through yellows to deep orange and eventually red
// *******************************************************
void cooldownAnimation() {
  
  // Calculate new value of proportion to go
  float prop = proportionToGo(cooldownStart, cooldownEnd);
  // Set brightness proportionally to time to go i.e get darker over time
  strip.setBrightness(255 * prop);
  // Update strip
  strip.show();

  // Update pixel colours
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 255 * prop/3, 0));
    strip.show();
  }
}

// ************************
// Update Time
//
// Updates the time globals
// ************************
void updateTime() {
  // Get update to timeclient
  timeClient.update();
  // Update current second global
  currentSecond = timeToSeconds(timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
}

// **********************************************************
// Time to Seconds
//
// Converts time values in h, minutes, and seconds to seconds
// **********************************************************
float timeToSeconds(float h, float m, float s) {
  float hoursInSeconds = h*60*60;
  float minutesInSeconds = m*60;
  float total = hoursInSeconds + minutesInSeconds + s;
  return total;
}

// In Period
// Checks if current time is between the two given times
bool inPeriod(float timeStart, float timeEnd) {
  if(timeStart <= currentSecond && currentSecond <= timeEnd) {
    return true;
  } else {
    return false;
  }
}

// ********************************************************
// Proportion Done
//
// Calculates the proportion of the time passed in a period
// ********************************************************
float proportionDone(float timeStart, float timeEnd) {
  float period = timeEnd - timeStart;
  float periodDone = currentSecond - timeStart;
  float prop = periodDone/period;
  return prop;
}

// *******************************************************
// Proportion To Go
//
// Calculates the proportion of the time to go in a period
// *******************************************************
float proportionToGo(float timeStart, float timeEnd) {
  return 1 - proportionDone(timeStart, timeEnd);
}

// *********
// MAIN CODE
// *********

// *****
// SETUP
// *****
void setup() {
  // Start wifi
  WiFi.mode(WIFI_STA);
  // Start serial
  Serial.begin(115200);
  // UDP start
  timeClient.begin();
  timeClient.setTimeOffset(43200);
  // Start NP
  strip.begin();
  strip.setBrightness(255); // 100 % brightness
  strip.show(); // All pixels off
  delay(1000);
  
  // Setting ESP into STATION mode only (no AP mode or dual mode)
  wifi_set_opmode(STATION_MODE);

  // Setup wifi
  struct station_config wifi_config;

  memset(&wifi_config, 0, sizeof(wifi_config));
  strcpy((char*)wifi_config.ssid, ssid);
  strcpy((char*)wifi_config.password, password);

  wifi_station_set_config(&wifi_config);
  wifi_set_macaddr(STATION_IF, target_esp_mac);
  
  wifi_station_set_wpa2_enterprise_auth(1);

  // Clean up to be sure no old data is still inside
  wifi_station_clear_cert_key();
  wifi_station_clear_enterprise_ca_cert();
  wifi_station_clear_enterprise_identity();
  wifi_station_clear_enterprise_username();
  wifi_station_clear_enterprise_password();
  wifi_station_clear_enterprise_new_password();
  
  wifi_station_set_enterprise_identity((uint8*)identity, strlen(identity));
  wifi_station_set_enterprise_username((uint8*)username, strlen(username));
  wifi_station_set_enterprise_password((uint8*)password, strlen((char*)password));
  
  wifi_station_connect();
  int i = 0; // Counter for strip
  while (WiFi.status() != WL_CONNECTED) {
    strip.setBrightness(20);
    delay(1000);
    strip.setPixelColor(i, strip.Color(255, 0, 0));
    strip.show();
    if(i >= strip.numPixels()) {
      // Clear every pixel
      for(int j = 0; j < strip.numPixels(); j++) {
        strip.setBrightness(0);
        strip.setPixelColor(j, strip.Color(0, 0, 0));
        strip.show();
      }
    }
    
    i++;
    Serial.print(".");
  }

  delay(1000);

  // Print connection
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Alarm times
  alarmStart = timeToSeconds(6, 0, 0);
  alarmEnd = timeToSeconds(8, 0, 0);
  // Alarm buffer times
  aBufferStart = alarmEnd;
  aBufferEnd = timeToSeconds(10, 00, 0);
  // Cooldown times
  cooldownStart = timeToSeconds(20, 00, 0);
  cooldownEnd = timeToSeconds(23, 30, 0);
}

// ****
// MAIN
// ****
void loop() {
  updateTime();
  
  if(inPeriod(alarmStart, alarmEnd)) {
    alarmAnimation();
    delay(500);
  } else if (inPeriod(aBufferStart, aBufferEnd)) {
    // Do nothing
    delay(1000);
  } else if (inPeriod(cooldownStart, cooldownEnd)) {
    cooldownAnimation();
    delay(500);
  } else if (inPeriod(aBufferEnd, cooldownStart)) {
    idleAnimation(cooldownStart);
  } else {
    // Set all pixels off
    for(int i = 0; i < strip.numPixels(); i++){
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(1000);
  }
}
