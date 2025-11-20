#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define SAMPLE_RATE 44100
#define AMPLITUDE 10000

typedef struct {
    char riff[4];
    int size;
    char wave[4];
    char fmt[4];
    int fmt_size;
    short audio_format;
    short num_channels;
    int sample_rate;
    int byte_rate;
    short block_align;
    short bits_per_sample;
    char data[4];
    int data_size;
} WAVHeader;

void write_wav_header(FILE *f, int num_samples) {
    WAVHeader header;
    memcpy(header.riff, "RIFF", 4);
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    memcpy(header.data, "data", 4);
    
    header.fmt_size = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = SAMPLE_RATE;
    header.bits_per_sample = 16;
    header.byte_rate = SAMPLE_RATE * header.num_channels * header.bits_per_sample / 8;
    header.block_align = header.num_channels * header.bits_per_sample / 8;
    header.data_size = num_samples * header.num_channels * header.bits_per_sample / 8;
    header.size = 36 + header.data_size;
    
    fwrite(&header, sizeof(WAVHeader), 1, f);
}

void add_tone(FILE *f, double freq, double duration) {
    int num_samples = (int)(SAMPLE_RATE * duration);
    for (int i = 0; i < num_samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        short sample = (short)(AMPLITUDE * sin(2.0 * M_PI * freq * t));
        fwrite(&sample, sizeof(short), 1, f);
    }
}

int main() {
    const char *filename = "/tmp/song.wav";
    FILE *f = fopen(filename, "wb");
    
    if (!f) {
        printf("Error creating audio file\n");
        return 1;
    }
    
    // Note frequencies
    double C = 261.63, D = 293.66, E = 329.63, F = 349.23;
    double G = 392.00, A = 440.00, rest = 0.0;
    double beat = 0.3;
    
    // Reserve space for header
    fseek(f, sizeof(WAVHeader), SEEK_SET);
    
    // Twinkle Twinkle Little Star melody
    double notes[] = {C,C,G,G,A,A,G,rest, F,F,E,E,D,D,C,rest,
                      G,G,F,F,E,E,D,rest, G,G,F,F,E,E,D,rest,
                      C,C,G,G,A,A,G,rest, F,F,E,E,D,D,C,rest};
    
    printf("🎵 Playing: Twinkle Twinkle Little Star 🎵\n");
    
    for (int i = 0; i < sizeof(notes)/sizeof(notes[0]); i++) {
        if (notes[i] > 0) {
            add_tone(f, notes[i], beat);
        } else {
            add_tone(f, 0, beat * 0.5);
        }
    }
    
    int num_samples = (ftell(f) - sizeof(WAVHeader)) / sizeof(short);
    fseek(f, 0, SEEK_SET);
    write_wav_header(f, num_samples);
    fclose(f);
    
    // Play the file using macOS's built-in afplay
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "afplay %s", filename);
    system(cmd);
    
    // Cleanup
    unlink(filename);
    
    return 0;
}
