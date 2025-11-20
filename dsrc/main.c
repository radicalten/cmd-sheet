#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// For Linux PC speaker control
#define KIOCSOUND 0x4B2F

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

void play_tone(int fd, int freq, int duration_ms) {
    if (freq > 0) {
        ioctl(fd, KIOCSOUND, 1193180 / freq);
    }
    usleep(duration_ms * 1000);
    ioctl(fd, KIOCSOUND, 0);
    usleep(30000); // Brief pause between notes
}

int main() {
    printf("🎵 Playing: Twinkle Twinkle Little Star 🎵\n\n");
    
    int fd = open("/dev/tty", O_WRONLY);
    if (fd == -1) {
        fd = open("/dev/console", O_WRONLY);
    }
    
    if (fd == -1) {
        printf("❌ Cannot access speaker. Try running with: sudo ./song\n");
        printf("📝 Showing visual music instead:\n\n");
        // Fallback: visual representation
        char *notes[] = {"C", "C", "G", "G", "A", "A", "G", "F", "F", "E", "E", "D", "D", "C"};
        for (int i = 0; i < 14; i++) {
            printf("♪ %s ", notes[i]);
            fflush(stdout);
            usleep(500000);
        }
        printf("\n");
        return 1;
    }
    
    // Twinkle Twinkle Little Star
    int song[][2] = {
        {C4, 500}, {C4, 500}, {G4, 500}, {G4, 500},
        {A4, 500}, {A4, 500}, {G4, 1000},
        {F4, 500}, {F4, 500}, {E4, 500}, {E4, 500},
        {D4, 500}, {D4, 500}, {C4, 1000}
    };
    
    char *lyrics[] = {
        "Twin", "kle", "twin", "kle",
        "lit", "tle", "star",
        "How", "I", "won", "der",
        "what", "you", "are"
    };
    
    int num_notes = sizeof(song) / sizeof(song[0]);
    
    for (int i = 0; i < num_notes; i++) {
        printf("♪ %s ", lyrics[i]);
        fflush(stdout);
        play_tone(fd, song[i][0], song[i][1]);
    }
    
    printf("\n\n✨ Done! ✨\n");
    close(fd);
    return 0;
}
