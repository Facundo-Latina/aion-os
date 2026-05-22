#pragma once
#include <stdint.h>
#include <stdbool.h>
void audio_init(void);
void audio_play_tone(uint32_t hz, uint32_t ms, uint8_t volume);
void audio_play_pcm(const uint8_t *data, uint32_t len, uint32_t sample_rate, uint8_t channels);
void audio_stop(void);
bool audio_is_playing(void);
