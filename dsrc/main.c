// figure8_racer.c
// Simple single-screen top-down racing game (sideways figure-8 track)
// Controls: A/D to steer, Q to quit
// Build on POSIX systems:  gcc -std=c99 -O2 figure8_racer.c -lm -o racer

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#define SCREEN_W 80
#define SCREEN_H 24

#define TRACK_W  60
#define TRACK_H  20

#define TRACK_OFFSET_X ((SCREEN_W - TRACK_W) / 2)
#define TRACK_OFFSET_Y 1

static struct termios orig_termios;
static int orig_fl;

static char track[TRACK_H][TRACK_W];
static unsigned char isRoad[TRACK_H][TRACK_W];

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, orig_fl);
    /* show cursor */
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    orig_fl = fcntl(STDIN_FILENO, F_GETFL);
    if (orig_fl == -1) {
        perror("fcntl");
        exit(1);
    }

    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    if (fcntl(STDIN_FILENO, F_SETFL, orig_fl | O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(1);
    }

    /* hide cursor and clear screen */
    write(STDOUT_FILENO, "\x1b[?25l\x1b[2J", 10);
}

void init_track(void) {
    int x, y;
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            track[y][x] = ' ';
            isRoad[y][x] = 0;
        }
    }

    /* Parameters for sideways figure-8 (infinity symbol) */
    double R = (TRACK_H - 4) / 2.0;      /* loop radius */
    double d = R * 1.5;                  /* distance between loop centers (< 2R) */
    double thickness = 1.2;              /* road half-width */

    double total = 2.0 * R + d;
    double margin = (TRACK_W - total) / 2.0;
    double cx1 = margin + R;
    double cx2 = cx1 + d;
    double cy  = TRACK_H / 2.0;

    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            double dx1 = x - cx1;
            double dy1 = y - cy;
            double dx2 = x - cx2;
            double dy2 = y - cy;
            double dist1 = sqrt(dx1 * dx1 + dy1 * dy1);
            double dist2 = sqrt(dx2 * dx2 + dy2 * dy2);

            if (fabs(dist1 - R) <= thickness || fabs(dist2 - R) <= thickness) {
                track[y][x] = '#';
                isRoad[y][x] = 1;
            }
        }
    }

    /* Strengthen the central crossover (horizontal band) */
    int x_start = (int)cx1;
    int x_end   = (int)cx2;
    int y_center = (int)cy;
    for (y = y_center - 1; y <= y_center + 1; ++y) {
        if (y < 0 || y >= TRACK_H) continue;
        for (x = x_start; x <= x_end; ++x) {
            if (x < 0 || x >= TRACK_W) continue;
            track[y][x] = '#';
            isRoad[y][x] = 1;
        }
    }
}

int on_track(double x, double y) {
    int ix = (int)floor(x);
    int iy = (int)floor(y);
    if (ix < 0 || ix >= TRACK_W || iy < 0 || iy >= TRACK_H)
        return 0;
    return isRoad[iy][ix];
}

void handle_input(double *turn, int *quit) {
    char buf[32];
    ssize_t n;
    *turn = 0.0;

    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == 'q' || c == 'Q') {
                *quit = 1;
            } else if (c == 'a' || c == 'A') {
                *turn -= 1.0;
            } else if (c == 'd' || c == 'D') {
                *turn += 1.0;
            }
        }
    }
    /* Ignore EAGAIN / EWOULDBLOCK */
}

void render(double carX, double carY, double elapsed, int crashed) {
    char screen[SCREEN_H][SCREEN_W + 1];
    int x, y;

    for (y = 0; y < SCREEN_H; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            screen[y][x] = ' ';
        }
        screen[y][SCREEN_W] = '\0';
    }

    /* Copy track */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            int sx = TRACK_OFFSET_X + x;
            int sy = TRACK_OFFSET_Y + y;
            if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H - 1) {
                screen[sy][sx] = track[y][x];
            }
        }
    }

    /* Place car */
    int carCellX = (int)floor(carX + 0.5);
    int carCellY = (int)floor(carY + 0.5);
    int sx = TRACK_OFFSET_X + carCellX;
    int sy = TRACK_OFFSET_Y + carCellY;
    if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H - 1) {
        screen[sy][sx] = '@';
    }

    /* Draw */
    write(STDOUT_FILENO, "\x1b[H", 3); /* move cursor to home */
    for (y = 0; y < SCREEN_H - 1; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            char c = screen[y][x];
            if (c == '@') {
                /* red car */
                write(STDOUT_FILENO, "\x1b[31m@\x1b[0m", 10);
            } else {
                write(STDOUT_FILENO, &c, 1);
            }
        }
        write(STDOUT_FILENO, "\n", 1);
    }

    char hud[128];
    if (!crashed) {
        snprintf(hud, sizeof(hud),
                 "A/D: steer   Q: quit   Time: %.1f s",
                 elapsed);
    } else {
        snprintf(hud, sizeof(hud),
                 "CRASHED after %.1f s. Press Q to quit.", elapsed);
    }
    write(STDOUT_FILENO, hud, strlen(hud));
    write(STDOUT_FILENO, "\x1b[K", 3); /* clear to end of line */
}

int main(void) {
    enable_raw_mode();
    init_track();

    const double DT = 0.05;        /* 20 FPS */
    const double SPEED = 7.0;      /* cells per second */
    const double TURN_RATE = 2.5;  /* radians/sec per unit of input */

    /* Starting position: left side of left loop, heading right */
    double R = (TRACK_H - 4) / 2.0;
    double d = R * 1.5;
    double total = 2.0 * R + d;
    double margin = (TRACK_W - total) / 2.0;
    double cx1 = margin + R;
    double cy  = TRACK_H / 2.0;

    double carX = cx1 - R + 0.5;
    double carY = cy + 0.5;
    double angle = 0.0;

    if (!on_track(carX, carY)) {
        /* Fallback: find any road cell */
        int found = 0;
        for (int y = 0; y < TRACK_H && !found; ++y) {
            for (int x = 0; x < TRACK_W && !found; ++x) {
                if (isRoad[y][x]) {
                    carX = x + 0.5;
                    carY = y + 0.5;
                    found = 1;
                }
            }
        }
    }

    double elapsed = 0.0;
    int quit = 0;
    int crashed = 0;

    while (!quit && !crashed) {
        double turnInput = 0.0;
        handle_input(&turnInput, &quit);
        angle += turnInput * TURN_RATE * DT;

        double dx = cos(angle) * SPEED * DT;
        double dy = sin(angle) * SPEED * DT;

        double newX = carX + dx;
        double newY = carY + dy;

        if (!on_track(newX, newY)) {
            crashed = 1;
            render(carX, carY, elapsed, crashed);
            break;
        }

        carX = newX;
        carY = newY;
        elapsed += DT;

        render(carX, carY, elapsed, crashed);
        usleep((useconds_t)(DT * 1000000.0));
    }

    /* Wait for Q after crash */
    while (!quit) {
        double dummyTurn = 0.0;
        handle_input(&dummyTurn, &quit);
        render(carX, carY, elapsed, crashed);
        usleep(100000);
    }

    /* disable_raw_mode() will be called by atexit */
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}
