#include "Arduino.h"
#include "WiFi.h"

#include "SpeakerFeature.h"

// Wifi
const char *ssid = "FPT-Telecom-Huy";
const char *password = "7210lecop";

#define LED_PIN 14

// ===== WiFi Connection =====
void connectWiFi()
{
  Serial.println("[WiFi] Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("ESP32-S3 READY");

  pinMode(LED_PIN, OUTPUT);

  connectWiFi();

  initSpeaker();
  digitalWrite(LED_PIN, LOW);

  playSpeaker("Xin chào, tôi là robot Wall Lee đây!", "vi");
  Serial.println("Wake word detected! Listening for command...");
  while (speakerIsPlaying())
  { // wait until greeting finishes
    speakerLoop();
    delay(10);
  }
}

void loop()
{
  // 2. Check for Serial input without "blocking" the rest of the code
  speakerLoop();
  if (Serial.available())
  {
    String res = Serial.readStringUntil('\n'); // Cleaner way to read input
    res.trim();                                // Remove whitespace/newlines

    if (res.length() > 0)
    {
      Serial.print("User asked: ");
      Serial.println(res);

      // Get the AI Answer
      String answer = askAIModel(res);
      Serial.print("AI Answer: ");
      Serial.println(answer);

      // Start the speech
      playSpeaker(answer.c_str());
    }
  }

  // // 3. Waiting for wake word
  // speakerLoop();
  // while (speakerIsPlaying())
  // { // wait until greeting finishes
  //   speakerLoop();
  //   delay(10);
  // }
  // playSpeaker("Iva?", "en");
  // while (speakerIsPlaying())
  // { // wait until greeting finishes
  //   speakerLoop();
  //   delay(10);
  // }
  // String text = recordingMicro();
  // playSpeaker("Hmmm", "en");
  // while (speakerIsPlaying())
  // { // wait until greeting finishes
  //   speakerLoop();
  //   delay(10);
  // }
  // text.trim();
  // text.toLowerCase();
  // Serial.print("User asked: ");
  // Serial.println(text);
  // if (text.length() > 0)
  // {
  //   // Get the AI Answer
  //   if (text.indexOf("tạm biệt") >= 0)
  //   {
  //     Serial.println("Shutting down...");
  //     playSpeaker("Tạm biệt! Hẹn gặp lại! Hãy gọi lại tôi khi cần nhé!", "vi");
  //     while (speakerIsPlaying())
  //     {
  //       speakerLoop();
  //       delay(10);
  //     }
  //     ESP.restart(); // or put ESP32 into deep sleep
  //   }

  //   // Otherwise, process AI command
  //   String answer = askAIModel(text);
  //   Serial.print("AI Answer: ");
  //   Serial.println(answer);
  //   // Start the speech
  //   Serial.flush(); // Wait until all Serial prints finish
  //   delay(50);      // give DAC/I2S time to prepare
  //   playSpeaker(answer.c_str(), "vi");
  //   while (speakerIsPlaying())
  //   {                // wait until playback finishes
  //     speakerLoop(); // keep processing speaker buffer
  //     delay(10);
  //   }
  // }
}
