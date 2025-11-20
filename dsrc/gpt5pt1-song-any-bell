/*
 * song.c - Single-file, dependency-free C "song" player for the terminal.
 *
 * It uses only the C standard library (stdio.h, time.h) and the terminal
 * bell character '\a'. Because standard C has no sound API, this program
 * plays a rhythm using repeated beeps (no control over pitch).
 *
 * Compile:
 *   cc song.c -o song      (or gcc/clang)
 *
 * Run:
 *   ./song
 *
 * If your terminal/system bell is disabled, you may not hear anything.
 */

#include <stdio.h>
#include <time.h>

typedef struct {
    int is_rest;      /* 0 = beep, 1 = silence */
    int duration_ms;  /* duration in milliseconds */
} Beat;

/* Busy-wait sleep using only standard C clock().
   This burns CPU but keeps the program fully portable. */
static void sleep_ms(int ms)
{
    clock_t start = clock();
    /* convert ms to clock ticks using double to avoid truncation issues */
    clock_t wait_ticks = (clock_t)((double)ms * (double)CLOCKS_PER_SEC / 1000.0);

    while ((clock() - start) < wait_ticks) {
        /* busy wait */
    }
}

/* Play one beat: either beep or rest for the given duration. */
static void play_beat(Beat b)
{
    if (!b.is_rest) {
        putchar('\a');      /* terminal bell */
        fflush(stdout);
    }
    sleep_ms(b.duration_ms);
}

int main(void)
{
    /* Twinkle Twinkle Little Star rhythm (no pitch, just timing).
       Q = quarter note duration in milliseconds. */
    const int Q = 300;

    Beat song[] = {
        /* C  C  G  G  A  A  G  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},

        /* F  F  E  E  D  D  C  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},

        /* G  G  F  F  E  E  D  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},

        /* G  G  F  F  E  E  D  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},

        /* C  C  G  G  A  A  G  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},

        /* F  F  E  E  D  D  C  - */
        {0, Q}, {0, Q}, {0, Q}, {0, Q},
        {0, Q}, {0, Q}, {0, 2*Q},
    };

    int num_beats = (int)(sizeof(song) / sizeof(song[0]));
    int i;

    printf("Playing song (if your terminal bell is enabled)...\n");

    for (i = 0; i < num_beats; ++i) {
        play_beat(song[i]);
        /* small gap between beats so they don't run together */
        sleep_ms(40);
    }

    printf("\nDone.\n");
    return 0;
}
