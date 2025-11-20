#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define FILENAME "mario.wav"

/* Note frequencies (Hz) - standard Arduino/NES values */
#define NOTE_C4  261
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520

int main() {
    /* Super Mario Bros overworld theme - the iconic part everyone knows */
    int melody[] = {
        NOTE_E7, NOTE_E7, 0, NOTE_E7, 0, NOTE_C7, NOTE_E7, 0,
        NOTE_G7, 0, 0, 0, NOTE_G6, 0, 0, 0,
        NOTE_C7, 0, 0, NOTE_G6, 0, 0, NOTE_E6, 0,
        0, NOTE_A6, 0, NOTE_B6, 0, NOTE_AS6, NOTE_A6, 0,
        NOTE_G6, NOTE_E7, NOTE_G7, NOTE_A7, 0, NOTE_F7, NOTE_G7, 0,
        NOTE_E7, 0, NOTE_C7, NOTE_D7, NOTE_B6, 0, 0
    };
    int tempo[] = {
        12,12,12,12, 12,12,12,12, 12,12,12,12, 12,12,12,12,
        12,12,12,12, 12,12,12,12, 12,12,12,12, 12,12,12,12,
        9, 9, 9, 12,12,12,12, 12,12,12,12, 12,12,12,12
    };

    int notes = sizeof(melody) / sizeof(melody[0]);

    /* Calculate total number of samples */
    long long total_samples = 0;
    for (int i = 0; i < notes; i++) {
        int noteDurationMs = 1450 / tempo[i];        /* speed - tweak 1450 if you want faster/slower */
        double noteSec = noteDurationMs / 1000.0;
        double pauseSec = noteSec * 0.25;            /* short pause between notes */
        total_samples += (long long)((noteSec + pauseSec) * SAMPLE_RATE);
    }

    FILE *f = fopen(FILENAME, "wb");
    if (!f) {
        perror("Failed to create WAV file");
        return 1;
    }

    /* Write WAV header (will be correct because we pre-calculated size) */
    unsigned int uint_val;
    unsigned short ushort_val;

    fwrite("RIFF", 1, 4, f);
    uint_val = 36 + (unsigned int)(total_samples * 2);
    fwrite(&uint_val, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    uint_val = 16;
    fwrite(&uint_val, 4, 1, f);
    ushort_val = 1;                     /* PCM */
    fwrite(&ushort_val, 2, 1, f);
    ushort_val = 1;                     /* mono */
    fwrite(&ushort_val, 2, 1, f);
    uint_val = SAMPLE_RATE;
    fwrite(&uint_val, 4, 1, f);
    uint_val = SAMPLE_RATE * 2;
    fwrite(&uint_val, 4, 1, f);
    ushort_val = 2;                     /* block align */
    fwrite(&ushort_val, 2, 1, f);
    ushort_val = 16;                    /* bits per sample */
    fwrite(&ushort_val, 2, 1, f);

    fwrite("data", 1, 4, f);
    uint_val = (unsigned int)(total_samples * 2);
    fwrite(&uint_val, 4, 1, f);

    /* Generate audio - square wave (very NES/Mario-like) */
    double phase = 0.0;
    for (int i = 0; i < notes; i++) {
        int frequency = melody[i];
        int noteDurationMs = 1450 / tempo[i];
        int noteSamples = (int)((noteDurationMs / 1000.0) * SAMPLE_RATE);
        int pauseSamples = (int)((noteDurationMs / 1000.0 * 0.25) * SAMPLE_RATE);

        for (int j = 0; j < noteSamples; j++) {
            short sample = 0;
            if (frequency > 0) {
                phase += 2.0 * M_PI * frequency / SAMPLE_RATE;
                if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
                sample = (sin(phase) >= 0 ? 11000 : -11000);   /* square wave, volume safe */
            }
            fwrite(&sample, sizeof(short), 1, f);
        }
        /* pause (silence) */
        short zero = 0;
        for (int j = 0; j < pauseSamples; j++) {
            fwrite(&zero, sizeof(short), 1, f);
        }
    }

    fclose(f);

    /* Play it and clean up */
    system("afplay mario.wav");
    remove("mario.wav");

    printf("Super Mario Bros theme played!\n");
    return 0;
}
