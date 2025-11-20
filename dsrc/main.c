#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>

// Note frequencies (Hz)
#define C4  262
#define D4  294
#define E4  330
#define F4  349
#define G4  392
#define A4  440
#define B4  494
#define C5  523
#define REST 0

// Duration (microseconds)
#define QUARTER 400000
#define HALF    800000
#define WHOLE   1600000

typedef struct {
    int freq;
    int duration;
} Note;

void play_note(int fd, int freq, int duration) {
    if (freq > 0) {
        ioctl(fd, KIOCSOUND, 1193180 / freq);
        usleep(duration);
        ioctl(fd, KIOCSOUND, 0);
    } else {
        usleep(duration);
    }
    usleep(50000); // Pause between notes
}

int main() {
    int fd = open("/dev/console", O_WRONLY);
    
    if (fd == -1) {
        fd = open("/dev/tty", O_WRONLY);
        if (fd == -1) {
            fprintf(stderr, "Error: Cannot access console.\n");
            fprintf(stderr, "Try running: sudo ./song\n");
            return 1;
        }
    }
    
    printf("🎵 Playing: Mary Had a Little Lamb\n");
    
    Note song[] = {
        {E4, QUARTER}, {D4, QUARTER}, {C4, QUARTER}, {D4, QUARTER},
        {E4, QUARTER}, {E4, QUARTER}, {E4, HALF},
        {D4, QUARTER}, {D4, QUARTER}, {D4, HALF},
        {E4, QUARTER}, {G4, QUARTER}, {G4, HALF},
        {E4, QUARTER}, {D4, QUARTER}, {C4, QUARTER}, {D4, QUARTER},
        {E4, QUARTER}, {E4, QUARTER}, {E4, QUARTER}, {E4, QUARTER},
        {D4, QUARTER}, {D4, QUARTER}, {E4, QUARTER}, {D4, QUARTER},
        {C4, WHOLE},
        {0, 0}
    };
    
    for (int i = 0; song[i].freq != 0 || song[i].duration != 0; i++) {
        play_note(fd, song[i].freq, song[i].duration);
    }
    
    close(fd);
    printf("✓ Done!\n");
    return 0;
}
