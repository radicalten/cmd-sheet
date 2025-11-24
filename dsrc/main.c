/*
 * Single-file ASCII top-down racing game (Super Sprint style)
 *
 * No external dependencies: just standard C library + basic OS headers.
 *
 * - One car, single-screen track, lap counter.
 * - Arrow keys to drive, 'q' to quit.
 *
 * Build on Linux/macOS:
 *     gcc supersprint.c -o supersprint -lm
 *
 * Build on Windows (MinGW / MSVC):
 *     gcc supersprint.c -o supersprint -lm
 *   or
 *     cl supersprint.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  include <conio.h>
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/types.h>
#  include <sys/time.h>
#  define SLEEP_MS(ms) usleep((ms)*1000)
#endif

#define TRACK_W 60
#define TRACK_H 20

#define FRAME_TIME_MS 40              /* ~25 FPS */
#define DT ((float)FRAME_TIME_MS/1000.0f)
#define MAX_LAPS 3

#define MAX_SPEED 1.4f
#define ACCEL_PULSE 0.18f             /* speed added per UP key press */
#define BRAKE_PULSE 0.25f             /* speed removed per DOWN key */
#define FRICTION 0.985f               /* multiply speed by this each frame */
#define TURN_STEP 0.16f               /* radians per LEFT/RIGHT press */

#define PI 3.14159265358979323846f

/* ---------------- Keyboard handling (cross-platform) ---------------- */

#ifdef _WIN32

static int read_key(void)
/* Returns:
 *   'U','D','L','R' for arrow keys
 *   regular ASCII for others
 *   0 if no key available
 */
{
    if (!_kbhit())
        return 0;

    int c = _getch();
    if (c == 0 || c == 224) {  /* special key */
        int c2 = _getch();
        switch (c2) {
        case 72: return 'U';   /* up */
        case 80: return 'D';   /* down */
        case 75: return 'L';   /* left */
        case 77: return 'R';   /* right */
        default: return 0;
        }
    }
    return c;
}

static void init_terminal(void) { /* nothing special */ }
static void restore_terminal(void) { /* nothing special */ }

#else  /* POSIX (Linux/macOS, etc.) */

static struct termios orig_termios;

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    /* show cursor again */
    printf("\x1b[?25h");
    fflush(stdout);
}

static void init_terminal(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); /* no echo, no line buffering */
    raw.c_cc[VMIN]  = 0;             /* non-blocking read */
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* Read a key in non-blocking mode.
 * Returns:
 *   'U','D','L','R' for arrow keys
 *   regular ASCII for others
 *   0 if no key available
 */
static int read_key(void)
{
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n != 1)
        return 0;

    if (c == '\x1b') { /* escape sequence (possibly arrow key) */
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return 0;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return 0;

        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A': return 'U';
            case 'B': return 'D';
            case 'C': return 'R';
            case 'D': return 'L';
            default: return 0;
            }
        }
        return 0;
    }

    return c;
}

#endif

/* ---------------- Track and car ---------------- */

typedef struct {
    float x, y;       /* position in track coordinates */
    float angle;      /* radians; 0 = up, PI/2 = right, etc. */
    float speed;      /* scalar speed */
    int   laps;
    int   on_start;   /* whether we are currently on start line */
} Car;

static void init_track(char track[TRACK_H][TRACK_W+1])
{
    int y, x;

    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            char c = ' ';

            /* Outer wall */
            if (y == 0 || y == TRACK_H - 1 || x == 0 || x == TRACK_W - 1) {
                c = '#';
            }
            /* Inner rectangular wall to make a ring-shaped track */
            else if (x >= 15 && x <= TRACK_W - 16 &&
                     y >= 4  && y <= TRACK_H - 5 &&
                     (y == 4 || y == TRACK_H - 5 || x == 15 || x == TRACK_W - 16)) {
                c = '#';
            }
            else {
                c = '.';
            }

            track[y][x] = c;
        }
        track[y][TRACK_W] = '\0';
    }

    /* Vertical start/finish line inside the bottom part of the ring */
    {
        int sx = TRACK_W / 2;   /* roughly center */
        int sy;
        for (sy = TRACK_H - 4; sy <= TRACK_H - 2; ++sy) {
            track[sy][sx] = 'S';
        }
    }
}

/* Choose a character to represent the car based on its angle */
static char car_glyph(float angle)
{
    float a = angle;
    while (a < 0.0f)      a += 2.0f * PI;
    while (a >= 2.0f*PI)  a -= 2.0f * PI;

    if (a < PI * 0.25f || a >= PI * 1.75f) return '^';
    if (a < PI * 0.75f)  return '>';
    if (a < PI * 1.25f)  return 'v';
    return '<';
}

/* ---------------- Main game ---------------- */

int main(void)
{
    char track[TRACK_H][TRACK_W+1];
    Car car;
    int running = 1;
    int frame = 0;

    init_track(track);

    /* Initial car position: near bottom, inside ring, pointing up */
    car.x = (float)(TRACK_W / 2 - 2);
    car.y = (float)(TRACK_H - 3);
    car.angle = 0.0f;   /* up */
    car.speed = 0.0f;
    car.laps  = 0;
    car.on_start = 0;

#ifdef _WIN32
    init_terminal(); /* does nothing, but keeps API symmetric */
#else
    init_terminal();
#endif

    /* Hide cursor and clear screen */
    printf("\x1b[?25l");
    printf("\x1b[2J\x1b[H");
    fflush(stdout);

    while (running) {
        int ch;

        /* Process all available key presses this frame */
        while ((ch = read_key()) != 0) {
            if (ch == 'q' || ch == 'Q') {
                running = 0;
            } else if (ch == 'U') {
                car.speed += ACCEL_PULSE;
                if (car.speed > MAX_SPEED) car.speed = MAX_SPEED;
            } else if (ch == 'D') {
                car.speed -= BRAKE_PULSE;
                if (car.speed < 0.0f) car.speed = 0.0f;
            } else if (ch == 'L') {
                car.angle -= TURN_STEP;
            } else if (ch == 'R') {
                car.angle += TURN_STEP;
            }
        }

        /* Apply friction */
        car.speed *= FRICTION;
        if (car.speed < 0.0f) car.speed = 0.0f;

        /* Normalize angle to [-PI, PI] to avoid big numbers */
        if (car.angle >  PI) car.angle -= 2.0f * PI;
        if (car.angle < -PI) car.angle += 2.0f * PI;

        /* Move car */
        {
            float old_x = car.x;
            float old_y = car.y;

            /* 0 rad = up => dx = sin(a), dy = -cos(a) */
            float dx = sinf(car.angle) * car.speed;
            float dy = -cosf(car.angle) * car.speed;

            car.x += dx;
            car.y += dy;

            int ix = (int)(car.x + 0.5f);
            int iy = (int)(car.y + 0.5f);

            /* Collision check */
            if (ix < 0 || ix >= TRACK_W ||
                iy < 0 || iy >= TRACK_H ||
                (track[iy][ix] != '.' && track[iy][ix] != 'S')) {
                /* Simple collision: stop and revert position */
                car.x = old_x;
                car.y = old_y;
                car.speed *= 0.3f;
            } else {
                /* Lap detection via start line 'S' */
                if (track[iy][ix] == 'S') {
                    if (!car.on_start && car.speed > 0.2f) {
                        car.laps++;
                        car.on_start = 1;
                    }
                } else {
                    car.on_start = 0;
                }
            }
        }

        /* Win condition */
        if (car.laps >= MAX_LAPS) {
            running = 0;
        }

        /* ---- Drawing ---- */

        /* Clear and move cursor home */
        printf("\x1b[2J\x1b[H");

        /* Simple HUD */
        {
            double elapsed = frame * (FRAME_TIME_MS / 1000.0);
            printf("ASCII Super Sprint (single car)\n");
            printf("Laps: %d/%d  Time: %.1f s  Speed: %.2f   Controls: arrows to drive, q to quit\n",
                   car.laps, MAX_LAPS, elapsed, car.speed);
        }

        /* Make a copy of the track and place car on it */
        {
            char buf[TRACK_H][TRACK_W+1];
            int y, x;
            for (y = 0; y < TRACK_H; ++y) {
                for (x = 0; x < TRACK_W; ++x) {
                    buf[y][x] = track[y][x];
                }
                buf[y][TRACK_W] = '\0';
            }

            int cx = (int)(car.x + 0.5f);
            int cy = (int)(car.y + 0.5f);
            if (cx >= 0 && cx < TRACK_W && cy >= 0 && cy < TRACK_H) {
                buf[cy][cx] = car_glyph(car.angle);
            }

            for (y = 0; y < TRACK_H; ++y) {
                printf("%s\n", buf[y]);
            }
        }

        fflush(stdout);

        /* Frame timing */
        SLEEP_MS(FRAME_TIME_MS);
        frame++;
    }

    /* Final screen */
    {
        double elapsed = frame * (FRAME_TIME_MS / 1000.0);
        printf("\x1b[2J\x1b[H");
        printf("Race finished!\n");
        printf("Laps: %d/%d  Time: %.2f s\n", car.laps, MAX_LAPS, elapsed);
        printf("Thanks for playing.\n");
        printf("\x1b[?25h"); /* show cursor */
        fflush(stdout);
    }

#ifndef _WIN32
    /* restore_terminal is already registered with atexit, but call explicitly
       here as well to ensure terminal is normal even if main returns early. */
    restore_terminal();
#endif

    return 0;
}
