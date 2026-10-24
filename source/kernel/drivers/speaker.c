#include "../include/speaker.h"
#include "../include/io.h"

void speaker_play_tone(uint32_t freq_hz) {
    if (freq_hz == 0) return;

    uint32_t div = 1193180 / freq_hz;

    // Set PIT channel 2 to mode 3 (square wave generator)
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    // Enable speaker and timer gate bits in port 0x61
    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void speaker_stop(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

static void speaker_delay(volatile uint32_t count) {
    while (count--) {
        io_wait();
    }
}

void speaker_beep(uint32_t freq_hz, uint32_t duration_ticks) {
    speaker_play_tone(freq_hz);
    speaker_delay(duration_ticks * 1000);
    speaker_stop();
}

void speaker_panic_siren(void) {
    // Distinctive alternating emergency beep
    for (int cycle = 0; cycle < 4; cycle++) {
        speaker_play_tone(880); // High A note
        speaker_delay(200000);
        speaker_play_tone(440); // Low A note
        speaker_delay(200000);
    }
    speaker_stop();
}
