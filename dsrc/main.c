/*
 * Single File Terminal Song (Tetris Theme / Korobeiniki)
 * 
 * COMPILATION:
 *   Linux/macOS:  gcc tetris.c -o tetris
 *   Windows:      cl tetris.c (or gcc tetris.c -o tetris.exe)
 *
 * DEPENDENCIES:
 *   Linux: Requires 'aplay' (part of alsa-utils, installed on almost all distros)
 *   Windows: No dependencies (uses built-in windows.h)
 *   macOS: Requires 'sox' installed (brew install sox) used as 'play' command. 
 *          (Native macOS audio via C without frameworks is extremely complex).
 */

#include <stdio.h>
#include <stdlib.h>

// Platform detection
#ifdef _WIN32
    #include <windows.h>
    #define IS_WINDOWS 1
#else
    #include <unistd.h>
    #define IS_WINDOWS 0
#endif

// Audio Settings
#define SAMPLE_RATE 8000

// Note Frequencies (Hz)
#define REST 0
#define G3   196
#define GS3  208
#define A3   220
#define B3   247
#define C4   262
#define D4   294
#define E4   330
#define F4   349
#define FS4  370
#define G4   392
#define GS4  415
#define A4   440
#define B4   494
#define C5   523
#define D5   587
#define E5   659
#define F5   698
#define G5   784
#define A5   880

// Note Duration Multipliers
#define Q 1.0  // Quarter
#define H 2.0  // Half
#define E 0.5  // Eighth
#define S 0.25 // Sixteenth

// The Song Data (Frequency, Duration Multiplier)
// Tetris Theme (Korobeiniki)
typedef struct {
    int freq;
    float duration;
} Note;

Note song[] = {
    {E5, Q}, {B4, E}, {C5, E}, {D5, Q}, {C5, E}, {B4, E},
    {A4, Q}, {A4, E}, {C5, E}, {E5, Q}, {D5, E}, {C5, E},
    {B4, Q}, {B4, E}, {C5, E}, {D5, Q}, {E5, Q},
    {C5, Q}, {A4, Q}, {A4, Q}, {REST, Q},
    
    {D5, Q}, {F5, E}, {A5, Q}, {G5, E}, {F5, E},
    {E5, Q}, {C5, E}, {E5, Q}, {D5, E}, {C5, E},
    {B4, Q}, {B4, E}, {C5, E}, {D5, Q}, {E5, Q},
    {C5, Q}, {A4, Q}, {A4, Q}, {REST, Q},

    // End of loop signal
    { -1, -1 } 
};

// Global file pointer for the pipe (Linux/Unix only)
FILE *audio_pipe = NULL;

void init_audio() {
    if (!IS_WINDOWS) {
        // On Linux, we pipe raw audio data to 'aplay'
        // -t raw: raw data
        // -r: sample rate
        // -f U8: Unsigned 8-bit format
        // -c 1: Mono
        
        // Try opening aplay (Linux standard)
        audio_pipe = popen("aplay -t raw -r 8000 -f U8 -c 1 -q 2>/dev/null", "w");
        
        // If aplay failed, try 'play' (SoX - common on macOS/Linux)
        if (!audio_pipe) {
            audio_pipe = popen("play -t raw -r 8000 -b 8 -c 1 -e unsigned-integer - -q 2>/dev/null", "w");
        }
        
        if (!audio_pipe) {
            printf("Error: Could not find 'aplay' or 'sox'. Audio might not play.\n");
        }
    }
}

void close_audio() {
    if (!IS_WINDOWS && audio_pipe) {
        pclose(audio_pipe);
    }
}

// Synthesize and play a tone
void play_tone(int freq, float duration_mult) {
    int base_ms = 500; // Tempo: Length of a quarter note in ms
    int duration_ms = (int)(base_ms * duration_mult);

    // Visualizer
    if(freq != REST) {
        printf("Playing: %d Hz \r", freq);
        fflush(stdout);
    } else {
        printf("...Rest...     \r");
        fflush(stdout);
    }

    if (IS_WINDOWS) {
        if (freq > 0) {
            Beep(freq, duration_ms);
        } else {
            Sleep(duration_ms);
        }
    } else {
        // Unix Audio Synthesis (Square Wave)
        if (audio_pipe) {
            int total_samples = (SAMPLE_RATE * duration_ms) / 1000;
            
            for (int i = 0; i < total_samples; i++) {
                unsigned char sample;
                if (freq == 0) {
                    sample = 128; // Silence (midpoint of U8)
                } else {
                    // Generate Square Wave
                    // We switch between 0 (low) and 255 (high) based on the frequency period
                    int period = SAMPLE_RATE / freq;
                    int half_period = period / 2;
                    sample = ((i / half_period) % 2) ? 200 : 55; // Volume slightly reduced from max
                }
                fputc(sample, audio_pipe);
            }
            // Flush to ensure timing stays roughly accurate
            fflush(audio_pipe); 
        } else {
            // Fallback if no audio backend found: just wait
            usleep(duration_ms * 1000);
        }
    }
}

int main() {
    printf("=== C Terminal Synth ===\n");
    printf("Playing: Tetris Theme\n");
    
    init_audio();

    int i = 0;
    while (song[i].freq != -1) {
        play_tone(song[i].freq, song[i].duration);
        // A tiny gap between notes makes it sound more distinct
        if (!IS_WINDOWS) play_tone(REST, 0.05); 
        i++;
    }

    close_audio();
    printf("\nDone!\n");
    return 0;
}
