#ifndef SPEAKER_FEATURE_H
#define SPEAKER_FEATURE_H
#include <Arduino.h>
#include "Audio.h"

// This tells other files "audio exists somewhere else, don't create a new one"
extern Audio audio;

void initSpeaker();
String askAIModel(const String &Question);
void speakerLoop();
void playSound();
bool speakerIsPlaying();

void playSpeaker(const String &text, const char *lang);
void playLongSpeaker(const String &text, const char *lang);

// Play a WAV file from the SD card
void playWavFile(const uint8_t *data, size_t length);

#endif
