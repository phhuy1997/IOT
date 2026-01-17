
#include "Arduino.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <SD_MMC.h>

#include <ArduinoJson.h>

#include "Audio.h"

const char *perplexity_token = "xxxx";
const char *temperature = "0.2";
const char *max_tokens = "300";

// I2S Pins for ESP32-S3
#define I2S_LRC 16
#define I2S_BCLK 5
#define I2S_DOUT 4

#define I2S_PORT I2S_NUM_0

Audio audio;
bool gSpeakerPlaying = false;

// #define SAMPLE_RATE 44100
// #define TONE_FREQUENCY 440

String escapeJSON(const String &s)
{
  String result = "";
  for (size_t i = 0; i < s.length(); i++)
  {
    char c = s[i];
    if (c == '\"')
      result += "\\\"";
    else if (c == '\\')
      result += "\\\\";
    else
      result += c;
  }
  return result;
}

void initSpeaker()
{
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(18);
}

void playSpeaker(const String &text, const char *lang)
{
  audio.stopSong();
  gSpeakerPlaying = true;
  audio.connecttospeech(text.c_str(), lang);
}

void playSound()
{
  audio.stopSong();
  audio.connecttoFS(SD_MMC, "/wall-e.wav");
}

void speakerLoop()
{
  audio.loop(); // ✅ wrapped access
  if (gSpeakerPlaying && !audio.isRunning())
  { // <-- hypothetical isRunning()
    gSpeakerPlaying = false;
  }
}

// Return true if speaker is playing
bool speakerIsPlaying()
{
  return gSpeakerPlaying;
}

String askAIModel(const String &Question)
{

  HTTPClient https;

  https.begin("https://api.perplexity.ai/chat/completions"); // Use WiFiClient
  https.addHeader("Content-Type", "application/json");
  String token_key = String("Bearer ") + perplexity_token;
  https.addHeader("Authorization", token_key);
  https.setTimeout(30000);

  String payload = String(
      "{"
      "\"model\":\"sonar\","
      "\"messages\":["
      "{"
      "\"role\":\"system\","
      "\"content\":\"You are an embedded robot which has a controller of esp32 and can do expression, motion, set alarm. Convert the Content output with JSON as {type: expression | motion | set_alarm; name: happy | sad | null; time: timestamp of the alarm | null; answer: string text maximum 30 words to answer user request without any citation}\""
      "},"
      "{"
      "\"role\":\"user\","
      "\"content\":\"" +
      escapeJSON(Question) + "\""
                             "}"
                             "],"
                             "\"temperature\":" +
      temperature + ","
                    "\"max_tokens\":" +
      max_tokens +
      "}");

  Serial.print("payload : ");
  Serial.println(payload);

  // start connection and send HTTP header
  int httpCode = https.POST(payload);

  Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

  // httpCode will be negative on error
  // file found at server
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
  {
    String result = https.getString();
    Serial.print("result : ");
    Serial.println(result);

    if (result.length() < 1 || !result)
    {
      Serial.println("Result too short, something went wrong!");
      https.end();
      return "Câu trả lời trống";
    }

    DynamicJsonDocument doc(8192);
    DynamicJsonDocument doc2(8192);

    // deserializeJson(doc, result);
    DeserializationError err = deserializeJson(doc, result);
    if (err)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(err.c_str());
      https.end();
      return "Có lỗi xảy ra rồi";
    }

    String Content = doc["choices"][0]["message"]["content"].as<String>();
    Content.replace("```json", "");
    Content.replace("```", "");
    Content.replace("**", "");
    Content.trim();
    Serial.print("Content : ");
    Serial.println(Content);
    if (Content.length() < 1 || !Content)
    {
      Serial.println("❌ Content missing");
      https.end();
      return "Có lỗi xảy ra 0";
    }

    DeserializationError err2 = deserializeJson(doc2, Content);
    if (err2)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(err2.c_str());
      https.end();
      return "Có lỗi xảy ra 1";
    }

    String Answer = doc2["answer"].as<String>();
    if (Answer.length() < 1 || !Answer)
    {
      Serial.println("❌ Answer missing");
      https.end();
      return "Có lỗi xảy ra 2";
    }

    Serial.print("Perplexity Says : ");
    Serial.println(Answer);
    https.end();
    return Answer;
  }
  else
  {
    Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    https.end();
    return "Có lỗi xảy ra 3";
  }
}
