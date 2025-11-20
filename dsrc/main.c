#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define VOLUME 0.5
#define TEMP_FILE "/tmp/mac_terminal_song.wav"

// Note Frequencies (Hz)
#define REST 0.00
#define G3   196.00
#define A3   220.00
#define B3   246.94
#define C4   261.63
#define D4   293.66
#define E4   329.63
#define F4   349.23
#define G4   392.00
#define GS4  415.30
#define A4   440.00
#define B4   493.88
#define C5   523.25
#define D5   587.33
#define E5   659.25

typedef struct {
    double freq;
    int duration_ms;
} Note;

// WAV Header Structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t overall_size;  // File size - 8
    char wave[4];           // "WAVE"
    char fmt_chunk_marker[4]; // "fmt "
    uint32_t length_of_fmt; // 16
    uint16_t format_type;   // 1 (PCM)
    uint16_t channels;      // 1
    uint32_t sample_rate;   // 44100
    uint32_t byterate;      // sample_rate * bits_per_sample * channels / 8
    uint16_t block_align;   // bits_per_sample * channels / 8
    uint16_t bits_per_sample;// 16
    char data_chunk_header[4];// "data"
    uint32_t data_size;     // data size
} WavHeader;

void write_wav_header(FILE *file, uint32_t total_samples) {
    WavHeader header;
    uint32_t data_sz = total_samples * sizeof(int16_t);

    memcpy(header.riff, "RIFF", 4);
    header.overall_size = 36 + data_sz;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt_chunk_marker, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1; // PCM
    header.channels = 1;    // Mono
    header.sample_rate = SAMPLE_RATE;
    header.bits_per_sample = 16;
    header.block_align = header.channels * header.bits_per_sample / 8;
    header.byterate = header.sample_rate * header.block_align;
    memcpy(header.data_chunk_header, "data", 4);
    header.data_size = data_sz;

    fwrite(&header, sizeof(WavHeader), 1, file);
}

int main() {
    printf("Generating audio...\n");

    FILE *f = fopen(TEMP_FILE, "wb");
    if (!f) {
        fprintf(stderr, "Error: Could not open temporary file for writing.\n");
        return 1;
    }

    // Tetris Theme (Korobeiniki) Data
    // Speed modifier (lower is faster)
    int s = 220; 
    
    Note melody[] = {
        {E5, 2*s}, {B4, s}, {C5, s}, {D5, 2*s}, {C5, s}, {B4, s},
        {A4, 2*s}, {A4, s}, {C5, s}, {E5, 2*s}, {D5, s}, {C5, s},
        {B4, 3*s}, {C5, s}, {D5, 2*s}, {E5, 2*s},
        {C5, 2*s}, {A4, 2*s}, {A4, 2*s}, {REST, s}, {REST, s},
        // Section B
        {D5, 3*s}, {F4, s}, {A5, 2*s}, {G5, s}, {F5, s},
        {E5, 3*s}, {C5, s}, {E5, 2*s}, {D5, s}, {C5, s},
        {B4, 2*s}, {B4, s}, {C5, s}, {D5, 2*s}, {E5, 2*s},
        {C5, 2*s}, {A4, 2*s}, {A4, 2*s}, {REST, 2*s}
    };

    int note_count = sizeof(melody) / sizeof(Note);
    uint32_t total_samples = 0;

    // Placeholder for header (we will overwrite later with correct size)
    WavHeader empty_header = {0};
    fwrite(&empty_header, sizeof(WavHeader), 1, f);

    // Generate Audio
    for (int i = 0; i < note_count; i++) {
        double freq = melody[i].freq;
        int samples_n = (int)((melody[i].duration_ms / 1000.0) * SAMPLE_RATE);
        total_samples += samples_n;

        for (int t = 0; t < samples_n; t++) {
            int16_t sample_val = 0;
            if (freq > 0) {
                // Sine wave formula
                double time = (double)t / SAMPLE_RATE;
                double val = sin(2.0 * M_PI * freq * time);

                // Simple Envelope (Fade in/out to prevent clicking)
                double envelope = 1.0;
                if (t < 500) envelope = t / 500.0; // Attack
                if (t > samples_n - 500) envelope = (samples_n - t) / 500.0; // Release
                
                sample_val = (int16_t)(val * 32767 * VOLUME * envelope);
            }
            fwrite(&sample_val, sizeof(int16_t), 1, f);
        }
    }

    // Go back to start and write real header with calculated size
    rewind(f);
    write_wav_header(f, total_samples);
    fclose(f);

    printf("Playing song (Tetris Theme)...\n");
    
    // Use macOS built-in audio player
    char command[256];
    snprintf(command, sizeof(command), "afplay %s", TEMP_FILE);
    system(command);

    // Cleanup
    remove(TEMP_FILE);
    printf("Done.\n");

    return 0;
}
