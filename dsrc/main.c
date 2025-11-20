// song.c
// Single-file C program that generates and plays a melody on macOS.
// Compile: clang song.c -o song -std=c11 -Wall -Wextra
// Run:     ./song

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define AMPLITUDE   0.4     // 0.0 to 1.0
#define GAP_SECONDS 0.02    // silence between notes

typedef struct {
    double freq;     // Hz, 0.0 = rest
    double duration; // seconds
} Note;

static void write_u32_le(FILE *f, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
    b[2] = (unsigned char)((v >> 16) & 0xFF);
    b[3] = (unsigned char)((v >> 24) & 0xFF);
    fwrite(b, 1, 4, f);
}

static void write_u16_le(FILE *f, uint16_t v) {
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
    fwrite(b, 1, 2, f);
}

static void write_sample(FILE *f, double value) {
    if (value > 1.0)  value = 1.0;
    if (value < -1.0) value = -1.0;
    int16_t s = (int16_t)(value * 32767.0);
    fwrite(&s, sizeof(s), 1, f);
}

int main(void) {
    // Frequencies for C4 major scale
    const double C4 = 261.63;
    const double D4 = 293.66;
    const double E4 = 329.63;
    const double F4 = 349.23;
    const double G4 = 392.00;
    const double A4 = 440.00;

    // "Twinkle Twinkle Little Star" (first two phrases)
    Note melody[] = {
        {C4, 0.5}, {C4, 0.5}, {G4, 0.5}, {G4, 0.5},
        {A4, 0.5}, {A4, 0.5}, {G4, 1.0},

        {F4, 0.5}, {F4, 0.5}, {E4, 0.5}, {E4, 0.5},
        {D4, 0.5}, {D4, 0.5}, {C4, 1.0},
    };
    const size_t melody_len = sizeof(melody) / sizeof(melody[0]);

    const char *filename = "song_tmp.wav";
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // Reserve space for 44-byte WAV header (we'll fill it in later)
    unsigned char header[44] = {0};
    if (fwrite(header, 1, 44, f) != 44) {
        perror("fwrite header placeholder");
        fclose(f);
        return 1;
    }

    size_t total_samples = 0;

    for (size_t n = 0; n < melody_len; ++n) {
        Note note = melody[n];
        int note_samples = (int)(note.duration * SAMPLE_RATE);
        if (note_samples < 1) note_samples = 1;

        int gap_samples = (int)(GAP_SECONDS * SAMPLE_RATE);

        if (note.freq > 0.0) {
            double phase = 0.0;
            double phase_inc = 2.0 * M_PI * note.freq / (double)SAMPLE_RATE;

            // Simple fade-in/out to avoid clicks
            int attack = SAMPLE_RATE * 0.01;  // 10 ms
            int release = attack;
            if (attack + release > note_samples) {
                attack = note_samples / 2;
                release = note_samples - attack;
            }

            for (int i = 0; i < note_samples; ++i) {
                double env = 1.0;
                if (i < attack) {
                    env = (double)i / (double)attack;
                } else if (i > note_samples - release) {
                    env = (double)(note_samples - i) / (double)release;
                }

                double sample = sin(phase) * AMPLITUDE * env;
                phase += phase_inc;
                write_sample(f, sample);
                total_samples++;
            }
        } else {
            // Rest: write silence
            for (int i = 0; i < note_samples; ++i) {
                write_sample(f, 0.0);
                total_samples++;
            }
        }

        // Gap (silence) between notes
        for (int i = 0; i < gap_samples; ++i) {
            write_sample(f, 0.0);
            total_samples++;
        }
    }

    // Now fill in the WAV header
    uint32_t data_size = (uint32_t)(total_samples * sizeof(int16_t));
    uint32_t chunk_size = 36 + data_size;
    uint16_t audio_format = 1;      // PCM
    uint16_t num_channels = 1;      // mono
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = SAMPLE_RATE * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;

    rewind(f);

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, chunk_size);
    fwrite("WAVE", 1, 4, f);

    // fmt subchunk
    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16); // Subchunk1Size for PCM
    write_u16_le(f, audio_format);
    write_u16_le(f, num_channels);
    write_u32_le(f, SAMPLE_RATE);
    write_u32_le(f, byte_rate);
    write_u16_le(f, block_align);
    write_u16_le(f, bits_per_sample);

    // data subchunk
    fwrite("data", 1, 4, f);
    write_u32_le(f, data_size);

    fclose(f);

    // Use macOS's built-in 'afplay' to play the WAV file
    int ret = system("afplay song_tmp.wav");
    if (ret == -1) {
        perror("system(afplay)");
    }

    // Clean up temporary file
    remove(filename);

    return 0;
}
