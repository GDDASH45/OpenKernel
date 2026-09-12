#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void sound_init(void);
void play_sound(uint32_t frequency);
void nosound(void);
void beep(uint32_t frequency, uint32_t duration_ms);

#endif