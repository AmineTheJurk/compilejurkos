#ifndef JURKOS_SPEAKER_H
#define JURKOS_SPEAKER_H

#include "types.h"

void speaker_play_tone(uint32_t freq_hz);
void speaker_stop(void);
void speaker_beep(uint32_t freq_hz, uint32_t duration_ticks);
void speaker_panic_siren(void);

#endif
