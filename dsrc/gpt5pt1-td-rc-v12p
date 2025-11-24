/*
 * Simple terminal figure-8 racing game (single screen, single file).
 *
 * Controls: Arrows or WASD to move, q to quit.
 *
 * Requirements:
 *  - POSIX terminal (Linux, macOS, etc.)
 *  - ANSI escape codes supported.
 *
 * Compile:
 *  gcc -std=c99 -Wall -O2 race.c -o race
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

#ifndef _WIN32  /* This program is POSIX-specific. */
#define ESC "\x1b"

#define TRACK_W      60
#define TRACK_H      20
#define TRACK_TOP     3   /* 1-based screen row where track starts */
#define TRACK_LEFT   10   /* 1-based screen column where track starts */
#define LAPS_TO_WIN   3

/* Global terminal state */
static struct termios orig_termios;
static int orig_fcntl_flags = -1;

/* Track and game state */
static char track[TRACK_H][TRACK_W];

static int car_x = 0, car_y = 0;
static int start_x = 0, start_y = 0;

static int laps_completed = 0;
static int race_started   = 0;
static int race_won       = 0;
static int race_quit      = 0;

static double race_start_time      = 0.0;
static double race_end_time        = 0.0;
static double last_lap_cross_time  = 0.0;

/* Get current time in seconds (fractional). */
static double now_seconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* Restore terminal settings and show cursor. */
static void disable_raw_mode(void)
{
    if (orig_fcntl_flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, orig_fcntl_flags);
    }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    /* Show cursor, reset attributes */
    printf(ESC "[?25h" ESC "[0m");
    fflush(stdout);
}

/* Enable raw mode for non-blocking, per-char input and hide cursor. */
static void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    struct termios raw = orig_termios;
    /* cfmakeraw sets canonical off, echo off, etc. */
    cfmakeraw(&raw);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    orig_fcntl_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (orig_fcntl_flags == -1) {
        orig_fcntl_flags = 0;
    }
    fcntl(STDIN_FILENO, F_SETFL, orig_fcntl_flags | O_NONBLOCK);

    /* Hide cursor */
    printf(ESC "[?25l");
    fflush(stdout);
}

/* Ensure terminal is restored on Ctrl+C or termination. */
static void handle_signal(int sig)
{
    (void)sig;
    disable_raw_mode();
    printf("\nInterrupted.\n");
    fflush(stdout);
    exit(1);
}

/* Move cursor to track coordinates and draw a character. */
static void draw_at_track(int x, int y, char ch)
{
    int screen_row = TRACK_TOP + y;
    int screen_col = TRACK_LEFT + x;
    printf(ESC "[%d;%dH%c", screen_row, screen_col, ch);
}

/* Build a sideways figure-8 track into the global 'track' array and
   select a start/finish line near the bottom of the central crossing. */
static void build_track(void)
{
    memset(track, ' ', sizeof(track));

    /* Track layout: two rectangular loops with a crossing in the center. */
    const int w   = 4;    /* approximate road thickness */
    const int xL0 = 3,  xL1 = 25; /* left loop bounds (x) */
    const int xR0 = 34, xR1 = 56; /* right loop bounds (x) */
    const int y0  = 2,  y1  = 17; /* vertical bounds for both loops */

    /* Left loop - horizontal bands (top and bottom edges). */
    for (int x = xL0; x <= xL1; x++) {
        for (int k = 0; k < w; k++) {
            int yt = y0 + k;
            int yb = y1 - k;
            if (yt >= 0 && yt < TRACK_H) track[yt][x] = '#';
            if (yb >= 0 && yb < TRACK_H) track[yb][x] = '#';
        }
    }

    /* Left loop - vertical bands (left and right edges). */
    for (int y = y0; y <= y1; y++) {
        for (int k = 0; k < w; k++) {
            int xl = xL0 + k;
            int xr = xL1 - k;
            if (xl >= 0 && xl < TRACK_W) track[y][xl] = '#';
            if (xr >= 0 && xr < TRACK_W) track[y][xr] = '#';
        }
    }

    /* Right loop - horizontal bands. */
    for (int x = xR0; x <= xR1; x++) {
        for (int k = 0; k < w; k++) {
            int yt = y0 + k;
            int yb = y1 - k;
            if (yt >= 0 && yt < TRACK_H) track[yt][x] = '#';
            if (yb >= 0 && yb < TRACK_H) track[yb][x] = '#';
        }
    }

    /* Right loop - vertical bands. */
    for (int y = y0; y <= y1; y++) {
        for (int k = 0; k < w; k++) {
            int xl = xR0 + k;
            int xr = xR1 - k;
            if (xl >= 0 && xl < TRACK_W) track[y][xl] = '#';
            if (xr >= 0 && xr < TRACK_W) track[y][xr] = '#';
        }
    }

    /* Central crossing to make it a figure-8. */
    int cy = (y0 + y1) / 2;
    int cx = (xL1 + xR0) / 2;

    /* Horizontal bar across the middle. */
    for (int x = xL1 - w; x <= xR0 + w; x++) {
        for (int k = 0; k < w; k++) {
            int yy = cy + k - w / 2;
            if (x >= 0 && x < TRACK_W && yy >= 0 && yy < TRACK_H) {
                track[yy][x] = '#';
            }
        }
    }

    /* Vertical bar across the middle. */
    for (int y = y0; y <= y1; y++) {
        for (int k = 0; k < w; k++) {
            int xx = cx + k - w / 2;
            if (xx >= 0 && xx < TRACK_W && y >= 0 && y < TRACK_H) {
                track[y][xx] = '#';
            }
        }
    }

    /* Define start/finish line near bottom of central vertical bar. */
    start_x = cx;
    start_y = y1 - 1;
    if (start_y >= TRACK_H) start_y = TRACK_H - 2;

    /* Ensure chosen start cell is on the road; if not, search nearby. */
    if (track[start_y][start_x] != '#') {
        int found = 0;
        for (int r = 1; r < 6 && !found; r++) {
            for (int dy = -r; dy <= r && !found; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    int yy = start_y + dy;
                    int xx = start_x + dx;
                    if (yy >= 0 && yy < TRACK_H && xx >= 0 && xx < TRACK_W) {
                        if (track[yy][xx] == '#') {
                            start_y = yy;
                            start_x = xx;
                            found = 1;
                        }
                    }
                }
            }
        }
    }

    /* Mark the start/finish line visually with '=' across the road width. */
    for (int dx = -1; dx <= 1; dx++) {
        int xx = start_x + dx;
        if (xx >= 0 && xx < TRACK_W) {
            track[start_y][xx] = '=';
        }
    }
}

/* Draw the static HUD and the track once. */
static void draw_static_ui_and_track(void)
{
    /* Clear screen and home cursor. */
    printf(ESC "[2J");
    printf(ESC "[H");

    /* HUD line (static text). */
    printf("Laps: 0/%d   Time: 0.00 s   Controls: Arrows/WASD, q=quit",
           LAPS_TO_WIN);

    /* Draw track area. */
    for (int y = 0; y < TRACK_H; y++) {
        int screen_row = TRACK_TOP + y;
        int screen_col = TRACK_LEFT;
        printf(ESC "[%d;%dH", screen_row, screen_col);
        for (int x = 0; x < TRACK_W; x++) {
            putchar(track[y][x]);
        }
    }

    fflush(stdout);
}

/* Update only the numeric HUD values in place (laps and time). */
static void update_hud(double elapsed)
{
    int laps_disp = race_started ? laps_completed : 0;
    double t = race_started ? elapsed : 0.0;
    if (t > 999.99) t = 999.99;

    /* "Laps: X/3..."  -> laps digit at column 7 (1-based). */
    printf(ESC "[1;7H%d", laps_disp);

    /* "Time: 0.00 s"  -> time starts at column 19. */
    printf(ESC "[1;19H%5.2f", t);

    fflush(stdout);
}

/* Attempt to move the car; handle lap counting if crossing the line. */
static void try_move(int dx, int dy)
{
    if (dx == 0 && dy == 0) return;

    int nx = car_x + dx;
    int ny = car_y + dy;

    if (nx < 0 || nx >= TRACK_W || ny < 0 || ny >= TRACK_H)
        return;

    char cell = track[ny][nx];
    /* Road cells are '#' and the start line '='. */
    if (!(cell == '#' || cell == '='))
        return;

    int old_x = car_x;
    int old_y = car_y;

    car_x = nx;
    car_y = ny;

    /* Redraw the previous cell's underlying track, and draw the car at new pos. */
    draw_at_track(old_x, old_y, track[old_y][old_x]);
    draw_at_track(car_x, car_y, 'A');
    fflush(stdout);

    /* Lap detection: crossing the start line downward. */
    if (old_y < start_y && car_y >= start_y) {
        if (car_x >= start_x - 1 && car_x <= start_x + 1) {
            double now = now_seconds();

            if (!race_started) {
                race_started = 1;
                race_start_time = now;
                last_lap_cross_time = now;
                laps_completed = 1;
            } else if (now - last_lap_cross_time > 1.5) {
                last_lap_cross_time = now;
                laps_completed++;

                if (laps_completed >= LAPS_TO_WIN) {
                    race_won = 1;
                    race_end_time = now;
                }
            }
        }
    }
}

/* Handle a single input character (including arrow-key escape sequences). */
static void handle_input_char(char ch)
{
    if (ch == 'q' || ch == 'Q') {
        race_quit = 1;
        return;
    }

    if (ch == 'w' || ch == 'W') {
        try_move(0, -1);
    } else if (ch == 's' || ch == 'S') {
        try_move(0, 1);
    } else if (ch == 'a' || ch == 'A') {
        try_move(-1, 0);
    } else if (ch == 'd' || ch == 'D') {
        try_move(1, 0);
    } else if (ch == '\x1b') {
        /* Possible arrow key: ESC [ A/B/C/D */
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;

        if (seq[0] == '[') {
            if      (seq[1] == 'A') try_move(0, -1); /* Up */
            else if (seq[1] == 'B') try_move(0,  1); /* Down */
            else if (seq[1] == 'C') try_move(1,  0); /* Right */
            else if (seq[1] == 'D') try_move(-1, 0); /* Left */
        }
    }
}

int main(void)
{
    /* On Windows this won't work; bail out with a message. */
#ifdef _WIN32
    fprintf(stderr, "This program uses POSIX terminal control and "
                    "is intended for Unix-like systems.\n");
    return 1;
#endif

    /* Unbuffered stdout for immediate screen updates. */
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    enable_raw_mode();

    build_track();

    /* Place the car just above the start/finish line. */
    car_x = start_x;
    car_y = start_y - 2;
    if (car_y < 0) car_y = start_y;

    laps_completed        = 0;
    race_started          = 0;
    race_won              = 0;
    race_quit             = 0;
    race_start_time       = 0.0;
    race_end_time         = 0.0;
    last_lap_cross_time   = 0.0;

    draw_static_ui_and_track();
    draw_at_track(car_x, car_y, 'A');
    fflush(stdout);

    /* Main game loop. */
    while (!race_quit && !race_won) {
        char ch;
        ssize_t n;

        /* Process all pending input bytes. */
        while ((n = read(STDIN_FILENO, &ch, 1)) == 1) {
            handle_input_char(ch);
        }

        /* Update HUD timer. */
        double elapsed = race_started ? (now_seconds() - race_start_time) : 0.0;
        update_hud(elapsed);

        /* Simple frame limiter (~60 FPS). */
        usleep(16000);
    }

    disable_raw_mode();

    if (race_won) {
        double total = race_end_time - race_start_time;
        if (total < 0) total = 0.0;

        /* Victory screen. */
        printf(ESC "[2J" ESC "[H");
        printf("You finished %d laps in %.2f seconds!\n\n",
               LAPS_TO_WIN, total);
        printf("Victory! Thanks for playing.\n");
        printf("Press Enter to exit.\n");
        fflush(stdout);

        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* wait for newline */
        }
    }

    return 0;
}

#else
int main(void)
{
    fprintf(stderr, "This program is intended for POSIX (Unix-like) systems.\n");
    return 1;
}
#endif
