#include "Arduino.h"
#include "WiFi.h"

#include "SpeakerFeature.h"
#include "Voice.h"
#include "WiFiHelper.h"
#include "FirebaseHelper.h"
#include <FirebaseESP32.h>

#define LED_PIN 14
// Firebase
// Provide the RTDB URL (IOT-ESP32-React-default-rtdb.asia-southeast1.firebasedatabase.app)
#define FIREBASE_HOST "iot-esp32-react-default-rtdb.asia-southeast1.firebasedatabase.app"
// Provide the Database Secret (found in Project settings -> Service accounts -> Database secrets)
#define FIREBASE_AUTH "tBkK9z0DJKkQKdsK6iakfvo54gmuU7sRcSrjm1Ly" // <<< IMPORTANT: REPLACE WITH YOUR SECRET

// Define Firebase Data objects
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

// Wifi
// const char* ssid = "Ks Newday";
// const char* password = "0987424348";
const char *ssid = "FPT-Telecom-Huy";
const char *password = "7210lecop";

void setup()
{
  Serial.begin(115200);

  Serial.println("ESP32-S3 READY");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Ensure the light is off initially

  // First prerequisites
  connectWiFi(ssid, password);

  // Firebase setup
  setupFirebase(firebaseData, firebaseAuth, firebaseConfig, FIREBASE_HOST, FIREBASE_AUTH);

  // Setup micro (without wakeup word detection (turnoff))
  initMicro();

  initSpeaker();

  // Setup micro (without wakeup word detection (turnoff))
  initMicro();

  // Setup micro (without wakeup word detection (turnoff))
  initMicro();

  playSpeaker("Hi, I'm Wall-e! How can I help you?", "en");
  Serial.println("Wake word detected! Listening for command...");
  while (speakerIsPlaying())
  {
    speakerLoop();
    delay(10);
  }
}

void loop()
{
  if (Firebase.ready())
  {
    // Check light state
    if (Firebase.getBool(firebaseData, "/light_001/isOn"))
    {
      bool lightState = firebaseData.boolData();
      digitalWrite(LED_PIN, lightState ? LOW : HIGH);
      Serial.printf("Light 1: ");
      Serial.println(lightState);
    }
    else
    {
      Serial.printf("Error Light 1: ");
      Serial.println(firebaseData.errorReason());
    }

    // Check micro ready state:
    if (Firebase.getBool(firebaseData, "/micro/ready"))
    {
      bool isReady = firebaseData.boolData();
      if (isReady == true)
      {
        for (int i = 0; i < 5; i++)
        {
          playSpeaker("Iva?", "en");
          while (speakerIsPlaying())
          { // wait until greeting finishes
            speakerLoop();
            delay(10);
          }
          String text = recordingMicro();
          playSpeaker("Hmmm", "en");
          while (speakerIsPlaying())
          { // wait until greeting finishes
            speakerLoop();
            delay(10);
          }
          text.trim();
          text.toLowerCase();
          Serial.print("User asked: ");
          Serial.println(text);
          if (text.length() > 0)
          {
            // Get the AI Answer
            if (text.indexOf("tạm biệt") >= 0)
            {
              Serial.println("Shutting down...");
              playSpeaker("Tạm biệt! Hẹn gặp lại! Hãy gọi lại tôi khi cần nhé!", "vi");
              while (speakerIsPlaying())
              {
                speakerLoop();
                delay(10);
              }
              // Stop the 5-loop early if user says goodbye
              break;
            }
            else
            {
              // Otherwise, process AI command
              String answer = askAIModel(text);
              Serial.print("AI Answer: ");
              Serial.println(answer);
              // Start the speech
              Serial.flush(); // Wait until all Serial prints finish
              delay(50);      // give DAC/I2S time to prepare
              playLongSpeaker(answer.c_str(), "vi");
              while (speakerIsPlaying())
              {                // wait until playback finishes
                speakerLoop(); // keep processing speaker buffer
                delay(10);
              }
            }
          }
        }

        // Speaker from Micro loop (5 times) finished → reset flag
        if (!Firebase.setBool(firebaseData, "/micro/ready", false))
        {
          Serial.print("Failed to reset micro/ready: ");
          Serial.println(firebaseData.errorReason());
        }
        else
        {
          Serial.println("micro/ready reset to false");
        }
      }
    }
    else
    {
      Serial.printf("Error Micro ready check: ");
      Serial.println(firebaseData.errorReason());
    }

    // Check content speaker state
    if (Firebase.getBool(firebaseData, "/ask/ready"))
    {
      bool isReady = firebaseData.boolData();
      if (isReady == true)
      {
        if (Firebase.getString(firebaseData, "/ask/content"))
        {
          String text = firebaseData.stringData();
          String answer = askAIModel(text);
          Serial.print("AI Answer: ");
          Serial.println(answer);
          // Start the speech
          Serial.flush(); // Wait until all Serial prints finish
          delay(50);      // give DAC/I2S time to prepare
          playLongSpeaker(answer.c_str(), "vi");
          while (speakerIsPlaying())
          {                // wait until playback finishes
            speakerLoop(); // keep processing speaker buffer
            delay(10);
          }
          // Speaker finished → reset flag
          if (!Firebase.setBool(firebaseData, "/ask/ready", false))
          {
            Serial.print("Failed to reset ask/ready: ");
            Serial.println(firebaseData.errorReason());
          }
          else
          {
            Serial.println("ask/ready reset to false");
          }
        }
      }
    }
    else
    {
      Serial.printf("Error Ask Ready: ");
      Serial.println(firebaseData.errorReason());
    }
  }
  else
  {
    Serial.printf("Firebase not ready");
  }
}
