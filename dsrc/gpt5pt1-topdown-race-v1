/*
 * Simple single-file top-down racing game in C (text/terminal).
 *
 * - No external libraries, only standard C + POSIX (termios, fcntl, unistd).
 * - Works on Unix-like systems in an ANSI-compatible terminal.
 * - Controls: A = left, D = right, Q = quit
 *
 * Compile (Linux/macOS):
 *   gcc -std=c99 -Wall -O2 racing.c -o racing
 *
 * Run:
 *   ./racing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 20
#define PLAYER_ROW    (SCREEN_HEIGHT - 2)

static struct termios orig_termios;
static int orig_fl = -1;

/* ----- Terminal handling ----- */

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    if (orig_fl != -1) {
        fcntl(STDIN_FILENO, F_SETFL, orig_fl);
    }
}

void enable_raw_mode(void) {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);

    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);  /* no echo, no canonical mode */
    raw.c_cc[VMIN] = 0;               /* non-blocking read */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    orig_fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (orig_fl == -1) {
        perror("fcntl(F_GETFL)");
        exit(1);
    }
    if (fcntl(STDIN_FILENO, F_SETFL, orig_fl | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL)");
        exit(1);
    }
}

int read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1)
        return (int)c;
    return -1;
}

void clear_screen(void) {
    /* Clear screen and move cursor to home */
    const char *seq = "\x1b[2J\x1b[H";
    write(STDOUT_FILENO, seq, 7);
}

/* ----- Track generation ----- */

void generate_row(char row[SCREEN_WIDTH], int road_center, int road_half) {
    int i;
    for (i = 0; i < SCREEN_WIDTH; i++) {
        if (i < road_center - road_half || i > road_center + road_half)
            row[i] = '#';   /* wall */
        else
            row[i] = ' ';   /* road */
    }
}

void add_obstacle(char row[SCREEN_WIDTH], int road_center, int road_half) {
    /* Random obstacle inside the road with some probability */
    if (rand() % 8 == 0) {  /* 1 in 8 chance per new row */
        int min = road_center - road_half + 1;
        int max = road_center + road_half - 1;
        if (min < 0) min = 0;
        if (max >= SCREEN_WIDTH) max = SCREEN_WIDTH - 1;
        if (max > min) {
            int x = min + rand() % (max - min + 1);
            row[x] = 'X';
        }
    }
}

void scroll_track(char track[SCREEN_HEIGHT][SCREEN_WIDTH],
                  int *road_center, int road_half) {
    int r;

    /* Move all rows down by one */
    for (r = SCREEN_HEIGHT - 1; r > 0; r--) {
        memcpy(track[r], track[r - 1], SCREEN_WIDTH);
    }

    /* Slight random curve of the road */
    int dir = (rand() % 3) - 1; /* -1, 0, or +1 */
    *road_center += dir;

    /* Keep road within screen bounds, leaving 1 char margin */
    if (*road_center - road_half < 1)
        *road_center = road_half + 1;
    if (*road_center + road_half > SCREEN_WIDTH - 2)
        *road_center = SCREEN_WIDTH - 2 - road_half;

    /* Generate new top row */
    generate_row(track[0], *road_center, road_half);
    add_obstacle(track[0], *road_center, road_half);
}

/* ----- Main game ----- */

int main(void) {
    char track[SCREEN_HEIGHT][SCREEN_WIDTH];
    char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
    int r, c;
    int road_center = SCREEN_WIDTH / 2;
    int road_half   = SCREEN_WIDTH / 5; /* road half-width */
    int player_x    = SCREEN_WIDTH / 2;
    int running     = 1;
    int score       = 0;

    srand((unsigned int)time(NULL));

    /* Initialize track */
    for (r = 0; r < SCREEN_HEIGHT; r++) {
        generate_row(track[r], road_center, road_half);
        if (r > 2)  /* some obstacles further up */
            add_obstacle(track[r], road_center, road_half);
    }

    enable_raw_mode();
    clear_screen();

    while (running) {
        /* Input: read all available keys this frame */
        int key;
        while ((key = read_key()) != -1) {
            if (key == 'a' || key == 'A') {
                player_x--;
            } else if (key == 'd' || key == 'D') {
                player_x++;
            } else if (key == 'q' || key == 'Q') {
                running = 0;
            }
        }

        if (player_x < 0) player_x = 0;
        if (player_x >= SCREEN_WIDTH) player_x = SCREEN_WIDTH - 1;

        /* Update track (scroll down, create new row at top) */
        scroll_track(track, &road_center, road_half);

        /* Check collision at player's row/column */
        if (track[PLAYER_ROW][player_x] != ' ') {
            running = 0;
        } else {
            score++;
        }

        /* Build frame buffer */
        for (r = 0; r < SCREEN_HEIGHT; r++) {
            memcpy(screen[r], track[r], SCREEN_WIDTH);
        }
        /* Draw player */
        screen[PLAYER_ROW][player_x] = 'A';

        /* Draw everything */
        clear_screen();
        for (r = 0; r < SCREEN_HEIGHT; r++) {
            fwrite(screen[r], 1, SCREEN_WIDTH, stdout);
            putchar('\n');
        }
        printf("Score: %d   Controls: A=left, D=right, Q=quit\n", score);
        fflush(stdout);

        /* Frame delay (~15 FPS) */
        usleep(70000);
    }

    clear_screen();
    printf("Game over! Final score: %d\n", score);
    return 0;
}
