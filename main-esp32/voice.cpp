// CODE MOI

#include <HTTPClient.h>
#include "driver/i2s_std.h"
#include <ArduinoJson.h>
#include <Hello_world_-_audio_classification_inferencing.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "SpeakerFeature.h"

i2s_chan_handle_t rx_chan;

// ===== ElevenLabs Configuration =====
const char *ELEVENLABS_API_KEY = "sk_7891a29f5a9000dcdc8323b27472898f1a9c55cb0d73c836";
const char *ELEVENLABS_ENDPOINT = "https://api.elevenlabs.io/v1/speech-to-text";

// ===== I2S Configuration (INMP441) =====
#define I2S_WS_PIN GPIO_NUM_1
#define I2S_SCK_PIN GPIO_NUM_2
#define I2S_SD_PIN GPIO_NUM_41
#define I2S_PORT I2S_NUM_1
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define CHANNELS 1 // Mono
#define DMA_BUFFER_SIZE 1024
#define RECORD_TIME 10 // Seconds
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW_MINE 1

// Audio buffer
int32_t i2s_buffer[DMA_BUFFER_SIZE];
size_t bytes_read = 0;

// /** Audio buffers, pointers and selectors */
typedef struct
{
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW_MINE);
static bool record_status = true;

void createWavHeader(uint8_t *header, int audio_size)
{
    // RIFF chunk
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';

    int file_size = 36 + audio_size;
    header[4] = file_size & 0xff;
    header[5] = (file_size >> 8) & 0xff;
    header[6] = (file_size >> 16) & 0xff;
    header[7] = (file_size >> 24) & 0xff;

    // WAVE format
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';

    // fmt subchunk
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    header[16] = 16;
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;

    // Audio format (PCM = 1)
    header[20] = 1;
    header[21] = 0;

    // Channels (Mono = 1)
    header[22] = 1;
    header[23] = 0;

    // Sample rate (16000 Hz)
    header[24] = SAMPLE_RATE & 0xff;
    header[25] = (SAMPLE_RATE >> 8) & 0xff;
    header[26] = (SAMPLE_RATE >> 16) & 0xff;
    header[27] = (SAMPLE_RATE >> 24) & 0xff;

    // Byte rate (sampleRate * bytesPerSample * channels)
    int byte_rate = SAMPLE_RATE * 2 * 1;
    header[28] = byte_rate & 0xff;
    header[29] = (byte_rate >> 8) & 0xff;
    header[30] = (byte_rate >> 16) & 0xff;
    header[31] = (byte_rate >> 24) & 0xff;

    // Block align
    header[32] = 2;
    header[33] = 0;

    // Bits per sample
    header[34] = 16;
    header[35] = 0;

    // data subchunk
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = audio_size & 0xff;
    header[41] = (audio_size >> 8) & 0xff;
    header[42] = (audio_size >> 16) & 0xff;
    header[43] = (audio_size >> 24) & 0xff;
}

// ===== I2S Initialization =====
void setupI2S()
{
    // Channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_PORT,
        I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // Standard I2S configuration
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD_PIN}};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    Serial.println("[I2S] Initialized (new i2s_std)");
}

static int i2s_deinit(void)
{
    if (rx_chan != NULL)
    {
        i2s_channel_disable(rx_chan);
        i2s_del_channel(rx_chan);
        rx_chan = NULL;
    }
    return 0;
}

static void audio_inference_callback(uint32_t n_bytes)
{
    for (int i = 0; i < n_bytes >> 1; i++)
    {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if (inference.buf_count >= inference.n_samples)
        {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void *arg)
{

    const int32_t i2s_bytes_to_read = (uint32_t)arg;

    while (record_status)
    {

        /* read data at once from i2s */
        esp_err_t ret = i2s_channel_read(
            rx_chan,
            (void *)sampleBuffer,
            i2s_bytes_to_read,
            &bytes_read,
            pdMS_TO_TICKS(100));

        if (ret != ESP_OK || bytes_read <= 0)
        {
            ei_printf("Error in I2S read : %d\n", (int)ret);
        }
        else
        {
            if (bytes_read < (size_t)i2s_bytes_to_read)
            {
                ei_printf("Partial I2S read");
            }

            // scale the data (otherwise the sound is too quiet)
            for (int x = 0; x < i2s_bytes_to_read / 2; x++)
            {
                sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
            }

            if (record_status)
            {
                audio_inference_callback(i2s_bytes_to_read);
            }
            else
            {
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL)
    {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[1] == NULL)
    {
        ei_free(inference.buffers[0]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    ei_sleep(100);

    record_status = true;

    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void *)sample_buffer_size, 10, NULL);

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1)
    {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW_MINE)\n");
        ret = false;
    }

    while (inference.buf_ready == 0)
    {
        delay(1);
    }

    inference.buf_ready = 0;
    return true;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    ei_free(inference.buffers[0]);
    ei_free(inference.buffers[1]);
}

// ===== Record Audio Data =====
void recordAudio(int16_t *pcm_buffer, size_t pcm_size)
{
    size_t total_bytes = 0;

    Serial.println("[Recording] Starting...");

    while (total_bytes < pcm_size)
    {

        size_t bytes_read = 0;

        ESP_ERROR_CHECK(
            i2s_channel_read(
                rx_chan,
                i2s_buffer,
                sizeof(i2s_buffer),
                &bytes_read,
                portMAX_DELAY));

        int samples_read = bytes_read / sizeof(int32_t);

        for (int i = 0; i < samples_read; i++)
        {
            if (total_bytes >= pcm_size)
                break;

            // Convert 32-bit → 16-bit (KEEP THIS)
            int16_t sample = (int16_t)(i2s_buffer[i] >> 14);

            ((int16_t *)((uint8_t *)pcm_buffer + total_bytes))[0] = sample;
            total_bytes += sizeof(int16_t);
        }

        Serial.print(".");
    }

    Serial.printf("\n[I2S] Recorded completed %u bytes\n", total_bytes);
}

// ===== Send to ElevenLabs =====
String getTranscripTextFromElevenLabs(uint8_t *audio_data, int audio_size)
{
    String transcription = "";
    HTTPClient https;

    // Boundary for multipart form
    String boundary = "----ESP32ElevenLabsBoundary";

    https.begin(ELEVENLABS_ENDPOINT);
    https.addHeader("xi-api-key", ELEVENLABS_API_KEY);

    https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    // Build multipart body
    String bodyStart =
        "--" + boundary + "\r\n"
                          "Content-Disposition: form-data; name=\"model_id\"\r\n\r\n"
                          "scribe_v1\r\n"
                          "--" +
        boundary + "\r\n"
                   "Content-Disposition: form-data; name=\"language_code\"\r\n\r\n"
                   "vie\r\n"
                   "--" +
        boundary + "\r\n"
                   "Content-Disposition: form-data; name=\"tag_audio_events\"\r\n\r\n"
                   "false\r\n"
                   "--" +
        boundary + "\r\n"
                   "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                   "Content-Type: audio/wav\r\n\r\n";

    String bodyEnd =
        "\r\n--" + boundary + "--\r\n";

    int contentLength = bodyStart.length() + audio_size + bodyEnd.length();
    https.addHeader("Content-Length", String(contentLength));

    // Allocate buffer for full request body
    uint8_t *requestBody = (uint8_t *)malloc(contentLength);
    if (!requestBody)
    {
        Serial.println("[ElevenLabs] Failed to allocate memory");
        https.end();
        return "";
    }

    // Copy multipart sections into buffer
    int offset = 0;
    memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length());
    offset += bodyStart.length();

    memcpy(requestBody + offset, audio_data, audio_size);
    offset += audio_size;

    memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());

    // Send POST request
    int httpCode = https.POST(requestBody, contentLength);
    free(requestBody);

    if (httpCode == 200)
    {
        String response = https.getString();
        Serial.println("[ElevenLabs] Response:");
        Serial.println(response);

        // Parse JSON
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, response);
        if (!err && doc.containsKey("text"))
        {
            transcription = doc["text"].as<String>();
            Serial.println("\n[Transcription]:");
            Serial.println(transcription);
        }
    }
    else
    {
        Serial.print("[ElevenLabs] Error: ");
        Serial.println(httpCode);
        Serial.println(https.getString());
    }

    https.end();
    return transcription;
}

void wakeupLoop()
{
    Serial.println("1");
    playSpeaker("Wall Lee", "en");
    while (speakerIsPlaying())
    {
        speakerLoop();
        delay(10);
    }

    bool recognized = false;
    Serial.println("2");

    // Loop here until a voice recognition condition is met
    while (!recognized)
    {
        Serial.println("3");
        bool m = microphone_inference_record();
        Serial.println("4");
        if (!m)
        {
            ei_printf("ERR: Failed to record audio...\n");
            return;
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
        signal.get_data = &microphone_audio_signal_get_data;
        ei_impulse_result_t result = {0};

        EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
        if (r != EI_IMPULSE_OK)
        {
            ei_printf("ERR: Failed to run classifier (%d)\n", r);
            return;
        }

        if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW_MINE))
        {
            // print the predictions
            ei_printf("Predictions ");
            ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
                      result.timing.dsp, result.timing.classification, result.timing.anomaly);
            ei_printf(": \n");
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
            {
                ei_printf("    %s: ", result.classification[ix].label);
                ei_printf_float(result.classification[ix].value);
                ei_printf("\n");
            }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
            ei_printf("    anomaly score: ");
            ei_printf_float(result.anomaly);
            ei_printf("\n");
#endif

            print_results = 0;
        }

        // Recognition condition: break the while-loop when any class
        // probability is above a chosen threshold (e.g. 0.8).
        // Optionally also check result.classification[ix].label for a specific word.
        const float RECOGNITION_THRESHOLD = 0.8f;
        if (result.classification[0].value > RECOGNITION_THRESHOLD)
        {
            recognized = true;
            break;
        }
    }

    // Stop capture task and free inference buffers now that wake word is detected
    record_status = false;
    microphone_inference_end();
}

// ===== Setup =====
void initMicro()
{
    delay(1000);
    Serial.println("\n\n[System] Initializing ESP32-S3 Speech-to-Text");
    // Initialize I2S once for both wake-word and later recording
    setupI2S();
    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false)
    {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }
    // wakeupLoop(); // PHN disabled wake word for no need to waiting for listening Machine learning to be wake and break the loop
    Serial.println("\n\n[System] Micro ready");
    delay(2000);
}

// ===== Main Loop =====
String recordingMicro()
{
    Serial.println("\n[System] Ready to record");

    static String transcription = "";

    const int pcm_samples = SAMPLE_RATE * RECORD_TIME;
    const int pcm_size = pcm_samples * sizeof(int16_t); // bytes

    int16_t *pcm_buffer = (int16_t *)malloc(pcm_size);
    if (!pcm_buffer)
    {
        Serial.println("[Error] PCM buffer allocation failed");
        return "";
    }

    memset(pcm_buffer, 0, pcm_size); // silence safety

    // ✅ Pass NUMBER OF SAMPLES, not bytes
    recordAudio(pcm_buffer, pcm_size);

    // ---- CREATE WAV BUFFER ----
    const int wav_size = 44 + pcm_size;
    uint8_t *wav_buffer = (uint8_t *)malloc(wav_size);
    if (!wav_buffer)
    {
        Serial.println("[Error] WAV buffer allocation failed");
        free(pcm_buffer);
        return "";
    }

    // ✅ Header must match real sample rate
    createWavHeader(wav_buffer, pcm_size);

    memcpy(wav_buffer + 44, (uint8_t *)pcm_buffer, pcm_size);

    // Send WAV to ElevenLabs
    transcription = getTranscripTextFromElevenLabs(wav_buffer, wav_size);

    Serial.print("transcription : ");
    Serial.println(transcription);

    free(pcm_buffer);
    free(wav_buffer);

    return transcription;
}
