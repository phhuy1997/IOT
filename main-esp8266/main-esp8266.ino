#include <Arduino.h>  
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>


#include "SpeakerFeature.h"


// #include <TFT_eSPI.h>
// #include <DasaiMochiPlayer.h>
// #include "video01.h"
// #include "video02.h"
// #include "video03.h"
// #include "video04.h"
// #include "video05.h"
// #include "video06.h"
// #include "video07.h"
// #include "video08.h" 
// #include "video09.h"
// #include "video10.h"
// #include "video11.h"
// #include "video12.h"
// #include "video13.h"
// #include "video14.h"


// Wifi
const char* ssid = "FPT-Telecom-Huy";
const char* password = "7210lecop";


// Video screen monitor
// TFT_eSPI tft;
// DasaiMochiPlayer player(tft);
// VideoInfo* videoList[] = {
//   &video01,
//  &video02,
//  &video03,
//  &video04,
//  &video05,
//  &video06,
//  &video07,
//  &video08

//   &video10,
//   &video11,
//   &video12,
//   &video13,
//   &video14
// };
// const uint8_t NUM_VIDEOS =
//   sizeof(videoList) / sizeof(videoList[0]);


bool speechPlayed = false;


// ===== WiFi Connection ===== 
void connectWiFi() { 
  Serial.println("[WiFi] Connecting..."); 
  WiFi.begin(ssid, password); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
} 




void setup() {
  Serial.begin(115200);

  connectWiFi();

 
  // initSpeaker();

  // // Init tft video screen
  // tft.init();
  // tft.setRotation(3);
  // tft.fillScreen(TFT_BLACK);
  // //BASIC:
  // // tft.setTextColor(TFT_WHITE);
  // // tft.drawString("ST7789 OK", 20, 20, 4);
  // // DASAI MOCHI PLAYER (use this):
  // tft.begin();
  // player.begin();
}

void loop() {
  String res = "";
  Serial.print("Ask your Question : ");
  while (!Serial.available());
  while (Serial.available())
  {
    char add = Serial.read();
    res = res + add;
    delay(1);
  }
  int len = res.length();
  res = res.substring(0, (len - 1));
  Serial.println(res);
  askAIModel(res);

  // delay(20000); 
  // Change face every 5 seconds (example)
  // player.playVideoList(videoList, NUM_VIDEOS, 20, 300);
}


