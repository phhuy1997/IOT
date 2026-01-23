#include "Arduino.h"
#include "WiFi.h"
#include <HTTPClient.h>
#include <SD_MMC.h>

#include <ArduinoJson.h>

#include "Audio.h"
#include "driver/i2s_std.h"

const char *perplexity_token = "xxxx";
const char *temperature = "0.2";
const char *max_tokens = "300";

// I2S Pins for ESP32-S3
#define I2S_LRC 16
#define I2S_BCLK 5
#define I2S_DOUT 4

#define I2S_PORT I2S_NUM_1

Audio audio;

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

// Return true if speaker is playing
bool speakerIsPlaying()
{
  return audio.isRunning();
}

void playSpeaker(const String &text, const char *lang)
{
  audio.stopSong();
  audio.connecttospeech(text.c_str(), lang);
}

void speakerLoop()
{
  // Let the Audio library manage its own running state
  audio.loop();
}

// New helper: play long text in multiple chunks so it won't be truncated
void playLongSpeaker(const String &fullText, const char *lang)
{
  // Use smaller chunks to avoid TTS length limits
  const int MAX_CHUNK_LEN = 80; // was 180

  int start = 0;
  int len = fullText.length();

  while (start < len)
  {
    int end = start + MAX_CHUNK_LEN;
    if (end > len)
      end = len;

    // Try to cut at a sentence boundary or space, not in the middle of a word
    int bestEnd = -1;
    for (int i = end - 1; i > start && i >= end - 50; --i)
    {
      char c = fullText[i];
      if (c == '.' || c == '!' || c == '?' || c == ',' || c == ' ')
      {
        bestEnd = i + 1;
        break;
      }
    }
    if (bestEnd != -1)
    {
      end = bestEnd;
    }

    String chunk = fullText.substring(start, end);
    chunk.trim();
    if (chunk.length() > 0)
    {
      playSpeaker(chunk, lang);
      while (speakerIsPlaying())
      {
        speakerLoop();
        delay(10);
      }
    }

    start = end;
    // skip extra spaces between chunks
    while (start < len && fullText[start] == ' ')
    {
      start++;
    }
  }
}

void playSound()
{
  audio.stopSong();
  audio.connecttoFS(SD_MMC, "/wall-e.wav");
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
      return Content;
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

// FOR PLAY WAV FILE FEATURE:
// Global handle for temporary raw PCM TX channel (new I2S driver)
static i2s_chan_handle_t tx_raw_pcm_chan = nullptr;

// Initialise I2S for raw PCM playback using
// BCLK (I2S_BCLK), LRC/WS (I2S_LRC) and DIN (I2S_DOUT).
// The amplifier's GAIN and SD pins are not handled by I2S
// and should be wired to fixed levels or separate GPIOs.
bool init_raw_pcm_i2s(uint32_t sampleRate)
{
  // Configure a TX-only standard I2S channel using the new driver
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);

  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_raw_pcm_chan, NULL);
  if (err != ESP_OK)
  {
    Serial.printf("[I2S-RAW] Failed to create I2S channel: %d\n", (int)err);
    tx_raw_pcm_chan = nullptr;
    return false;
  }

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = GPIO_NUM_5,
              .ws = GPIO_NUM_16,
              .dout = GPIO_NUM_4,
              .din = I2S_GPIO_UNUSED,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  err = i2s_channel_init_std_mode(tx_raw_pcm_chan, &std_cfg);
  if (err != ESP_OK)
  {
    Serial.printf("[I2S-RAW] Failed to init std mode: %d\n", (int)err);
    i2s_del_channel(tx_raw_pcm_chan);
    tx_raw_pcm_chan = nullptr;
    return false;
  }

  err = i2s_channel_enable(tx_raw_pcm_chan);
  if (err != ESP_OK)
  {
    Serial.printf("[I2S-RAW] Failed to enable channel: %d\n", (int)err);
    i2s_del_channel(tx_raw_pcm_chan);
    tx_raw_pcm_chan = nullptr;
    return false;
  }

  return true;
}

// Function to play an embedded WAV/PCM buffer
void playWavFile(const uint8_t *data, size_t length)
{
  uint32_t sampleRate = 48000;

  if (!init_raw_pcm_i2s(sampleRate))
  {
    return;
  }

  int16_t buffer[512];
  size_t index = 0;

  while (index < length)
  {
    size_t remaining = length - index;
    size_t n_samples = (remaining > sizeof(buffer) / sizeof(buffer[0])) ? sizeof(buffer) / sizeof(buffer[0]) : remaining;

    // Cast the uint8_t pointer to signed char pointer
    const signed char *signed_data = (const signed char *)data;

    for (size_t i = 0; i < n_samples; ++i)
    {
      signed char v = signed_data[index + i]; // Already signed (-128 to +127)
      int16_t sample = (int16_t)v << 8;       // Scale to 16-bit: -32768 to +32512
      sample /= 5;                            // 20% volume
      buffer[i] = sample;
    }

    size_t bytes_to_write = n_samples * sizeof(int16_t);
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_raw_pcm_chan, buffer, bytes_to_write, &bytes_written, pdMS_TO_TICKS(1000));
    if (err != ESP_OK)
    {
      Serial.printf("[I2S-RAW] i2s_channel_write failed: %d\n", (int)err);
      break;
    }

    index += n_samples;
  }

  delay(10);

  if (tx_raw_pcm_chan != nullptr)
  {
    i2s_channel_disable(tx_raw_pcm_chan);
    i2s_del_channel(tx_raw_pcm_chan);
    tx_raw_pcm_chan = nullptr;
  }

  initSpeaker();
}
