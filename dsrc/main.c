#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <time.h>

#define CLOCK_TICK_RATE 1193180

// Note frequencies in Hz
#define C4  262
#define D4  294
#define E4  330
#define F4  349
#define G4  392
#define A4  440
#define B4  494
#define C5  523
#define REST 0

// Note duration (milliseconds)
#define QUARTER 400
#define HALF    800

typedef struct {
    int freq;
    int duration;
} Note;

void play_note(int console_fd, int freq, int duration_ms) {
    if (freq > 0) {
        int count = CLOCK_TICK_RATE / freq;
        ioctl(console_fd, KIOCSOUND, count);
    }
    usleep(duration_ms * 1000);
    ioctl(console_fd, KIOCSOUND, 0); // Stop sound
    usleep(50000); // Small gap between notes
}

int main() {
    // Twinkle Twinkle Little Star
    Note song[] = {
        {C4, QUARTER}, {C4, QUARTER}, {G4, QUARTER}, {G4, QUARTER},
        {A4, QUARTER}, {A4, QUARTER}, {G4, HALF},
        {F4, QUARTER}, {F4, QUARTER}, {E4, QUARTER}, {E4, QUARTER},
        {D4, QUARTER}, {D4, QUARTER}, {C4, HALF},
        {G4, QUARTER}, {G4, QUARTER}, {F4, QUARTER}, {F4, QUARTER},
        {E4, QUARTER}, {E4, QUARTER}, {D4, HALF},
        {G4, QUARTER}, {G4, QUARTER}, {F4, QUARTER}, {F4, QUARTER},
        {E4, QUARTER}, {E4, QUARTER}, {D4, HALF},
        {C4, QUARTER}, {C4, QUARTER}, {G4, QUARTER}, {G4, QUARTER},
        {A4, QUARTER}, {A4, QUARTER}, {G4, HALF},
        {F4, QUARTER}, {F4, QUARTER}, {E4, QUARTER}, {E4, QUARTER},
        {D4, QUARTER}, {D4, QUARTER}, {C4, HALF},
    };

    int num_notes = sizeof(song) / sizeof(Note);
    
    int console_fd = open("/dev/console", O_WRONLY);
    if (console_fd == -1) {
        console_fd = open("/dev/tty", O_WRONLY);
        if (console_fd == -1) {
            fprintf(stderr, "Error: Cannot open console.\n");
            fprintf(stderr, "Try running with: sudo %s\n", __FILE__);
            return 1;
        }
    }

    printf("🎵 Playing: Twinkle Twinkle Little Star 🎵\n\n");
    
    for (int i = 0; i < num_notes; i++) {
        printf("♪ ");
        fflush(stdout);
        play_note(console_fd, song[i].freq, song[i].duration);
    }
    
    printf("\n\n🎵 Done! 🎵\n");
    
    close(console_fd);
    return 0;
}
