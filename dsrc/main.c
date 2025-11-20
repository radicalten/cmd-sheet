#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* 
 * MATH MUSIC (BYTEBEAT)
 * No external libraries required for compilation.
 * 
 * On Linux: Automatically attempts to pipe to 'aplay'.
 * On macOS: Requires 'sox' (install via: brew install sox) or manual piping.
 * On Windows: Compile with MinGW, runs if you have 'sox' or similar in PATH.
 */

int main() {
    // The command to pipe raw audio to.
    // Linux default: aplay (Standard ALSA player)
    // macOS/Windows: Try 'sox' if installed, otherwise this auto-play might fail.
    #if defined(__APPLE__)
        const char *player_cmd = "sox -t raw -b 8 -r 8000 -e unsigned - -d 2>/dev/null";
    #else
        const char *player_cmd = "aplay -f U8 -r 8000 -q 2>/dev/null";
    #endif

    FILE *audio_pipe = popen(player_cmd, "w");
    
    // If we can't open the pipe automatically, fall back to stdout
    // so the user can pipe it manually: ./song | aplay
    FILE *out = audio_pipe ? audio_pipe : stdout;

    // If printing to terminal (no pipe found), warn the user to avoid binary garbage
    if (!audio_pipe && isatty(fileno(stdout))) {
        fprintf(stderr, "Error: Could not find an audio player (aplay/sox).\n");
        fprintf(stderr, "To play, pipe this program manually:\n");
        fprintf(stderr, "  Linux: ./song | aplay -f U8 -r 8000\n");
        fprintf(stderr, "  Mac:   ./song | sox -t raw -b 8 -r 8000 -e unsigned - -d\n");
        return 1;
    }

    fprintf(stderr, "Playing... (Press Ctrl+C to stop)\n");

    // THE SONG LOOP
    // t = time iterator
    for (uint32_t t = 0; ; t++) {
        
        // --- THE MELODY EQUATION ---
        // This formula generates a sawtooth wave melody using bitwise operations.
        // Rate: 8000Hz, 8-bit depth.
        uint8_t sample = t * (t >> 11 & t >> 8 & 123 & t >> 3);
        
        // Write the byte to the audio stream
        fputc(sample, out);

        // TERMINAL VISUALIZER (Draws to stderr to not corrupt audio stream)
        // Update every 2000 samples (~4 times per second)
        if (t % 2000 == 0) {
            int vol = sample / 8; // Scale 0-255 down to 0-30ish for text
            fprintf(stderr, "\r[");
            for (int i = 0; i < 32; i++) {
                fprintf(stderr, "%c", i < vol ? '|' : ' ');
            }
            fprintf(stderr, "] %d", sample);
        }
    }

    if (audio_pipe) pclose(audio_pipe);
    return 0;
}
