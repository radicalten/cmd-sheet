/*
 * ASCII Super Sprint - single-file C, no external libraries.
 *
 * Controls:
 *   W = accelerate
 *   S = brake / reverse
 *   A = steer left
 *   D = steer right
 *   Q = quit
 *
 * Notes:
 *   - Uses ANSI escape codes for clearing the screen & hiding cursor.
 *   - On Windows 10+ this usually works out of the box. On very old
 *     consoles you may need to enable VT processing or it may flicker.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#  include <conio.h>
#  include <windows.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* Track dimensions (characters) */
#define TRACK_W 60
#define TRACK_H 20

/* Simulation parameters */
#define FRAME_MS   16        /* ~60 FPS */
#define DT         0.016f    /* seconds per frame */
#define MAX_LAPS   3

/* Finish line placement (must match init_track) */
#define FINISH_X   10
#define FINISH_Y1  6
#define FINISH_Y2  (TRACK_H - 7)

/* Global track definition */
static char track[TRACK_H][TRACK_W];

/* Car state */
typedef struct {
    float x, y;     /* position in track coordinates (columns, rows) */
    float speed;    /* scalar speed, can be negative (reverse) */
    float angle;    /* facing angle in radians, 0 = right, pi/2 = down */
} Car;

#ifdef _WIN32

static void sleep_ms(int ms) {
    Sleep(ms);
}

static int getch_nowait(void) {
    if (_kbhit())
        return _getch();
    return -1;
}

#else /* POSIX ---------------------------------------------------- */

static struct termios orig_termios;
static bool termios_initialized = false;

static void disable_raw_mode(void) {
    if (termios_initialized) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        termios_initialized = false;
    }
}

static void enable_raw_mode(void) {
    if (termios_initialized)
        return;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    termios_initialized = true;
    atexit(disable_raw_mode);
}

static int getch_nowait(void) {
    unsigned char c;
    int n = (int)read(STDIN_FILENO, &c, 1);
    if (n == 1)
        return (int)c;
    return -1;
}

static void sleep_ms(int ms) {
    usleep(ms * 1000);
}

#endif /* POSIX end ------------------------------------------------ */

/* Cursor visibility */
static void hide_cursor(void) {
    printf("\x1b[?25l");
    fflush(stdout);
}

static void show_cursor(void) {
    printf("\x1b[?25h");
    fflush(stdout);
}

/* Initialize a simple single-screen track with a central island and a finish line. */
static void init_track(void) {
    int x, y;

    /* Base: outer walls, interior drivable '.' */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            if (x == 0 || x == TRACK_W - 1 || y == 0 || y == TRACK_H - 1)
                track[y][x] = '#';    /* outer wall */
            else
                track[y][x] = '.';    /* road */
        }
    }

    /* Central island to force a loop */
    int island_left  = 20;
    int island_right = TRACK_W - 20;        /* 40 when TRACK_W=60 */
    int island_top   = 4;
    int island_bot   = TRACK_H - 5;         /* 15 when TRACK_H=20 */

    for (y = island_top; y <= island_bot; ++y) {
        for (x = island_left; x <= island_right; ++x) {
            track[y][x] = '#';
        }
    }

    /* Finish line: vertical stripe on the left corridor */
    for (y = FINISH_Y1; y <= FINISH_Y2; ++y) {
        track[y][FINISH_X] = '|';
    }
}

/* Car physics update: acceleration, friction, turning, movement */
static void update_car(Car *car, float dt, int accel, int brake, int left, int right) {
    const float MAX_SPEED     = 18.0f;  /* cells per second */
    const float ACCELERATION  = 25.0f;  /* accel strength */
    const float BRAKE_POWER   = 35.0f;  /* brake strength */
    const float FRICTION      = 8.0f;   /* natural drag */
    const float BASE_TURN_RATE= 2.8f;   /* radians per second */

    /* Longitudinal acceleration/braking */
    if (accel)
        car->speed += ACCELERATION * dt;
    if (brake)
        car->speed -= BRAKE_POWER * dt;

    /* Clamp forward/backward speeds */
    if (car->speed > MAX_SPEED)
        car->speed = MAX_SPEED;
    if (car->speed < -MAX_SPEED * 0.5f)
        car->speed = -MAX_SPEED * 0.5f;

    /* Friction */
    if (car->speed > 0.0f) {
        car->speed -= FRICTION * dt;
        if (car->speed < 0.0f)
            car->speed = 0.0f;
    } else if (car->speed < 0.0f) {
        car->speed += FRICTION * dt;
        if (car->speed > 0.0f)
            car->speed = 0.0f;
    }

    /* Turning: stronger effect at higher speed */
    float turn_factor = 0.3f + 0.7f * (fabsf(car->speed) / MAX_SPEED);
    float turn_amount = BASE_TURN_RATE * turn_factor * dt;

    if (left)
        car->angle -= turn_amount;
    if (right)
        car->angle += turn_amount;

    /* Keep angle in reasonable range */
    if (car->angle > (float)M_PI)
        car->angle -= 2.0f * (float)M_PI;
    else if (car->angle < -(float)M_PI)
        car->angle += 2.0f * (float)M_PI;

    /* Move forward along heading */
    car->x += cosf(car->angle) * car->speed * dt;
    car->y += sinf(car->angle) * car->speed * dt;
}

/* Render track + car */
static void draw(const Car *car, int laps_done, float total_time) {
    char buf[TRACK_H][TRACK_W];
    int x, y;

    /* Copy track to buffer */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            buf[y][x] = track[y][x];
        }
    }

    /* Overlay car */
    int cx = (int)(car->x + 0.5f);
    int cy = (int)(car->y + 0.5f);

    if (cx >= 0 && cx < TRACK_W && cy >= 0 && cy < TRACK_H) {
        buf[cy][cx] = '1'; /* player car */
    }

    /* Clear screen & move cursor home */
    printf("\x1b[2J\x1b[H");

    /* Simple HUD */
    printf("ASCII Super Sprint  |  WASD: drive   Q: quit\n");
    if (laps_done < MAX_LAPS) {
        printf("Lap: %d / %d   Time: %.1f s   Speed: %.1f\n\n",
               laps_done + 1, MAX_LAPS, total_time, fabsf(car->speed));
    } else {
        printf("Lap: %d / %d   Time: %.1f s   Speed: %.1f\n\n",
               MAX_LAPS, MAX_LAPS, total_time, fabsf(car->speed));
    }

    /* Print track */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < TRACK_W; ++x) {
            putchar(buf[y][x]);
        }
        putchar('\n');
    }

    fflush(stdout);
}

int main(void) {
#ifdef _WIN32
    /* Windows: console mode is already suitable for _getch/_kbhit */
#else
    enable_raw_mode();
#endif

    atexit(show_cursor);
    hide_cursor();

    init_track();

    Car car;
    car.x = 5.0f;                                       /* start left of finish line */
    car.y = (FINISH_Y1 + FINISH_Y2) * 0.5f;             /* centered vertically on it */
    car.speed = 0.0f;
    car.angle = 0.0f;                                   /* facing right */

    float total_time = 0.0f;
    float lap_times[MAX_LAPS] = {0};
    float last_lap_time = 0.0f;
    int   laps_done = 0;
    bool  race_finished = false;
    bool  quit = false;

    while (!quit && !race_finished) {
        int accel = 0, brake = 0, left = 0, right = 0;

        /* Accumulate time (fixed timestep) */
        total_time += DT;

        /* Read all keypresses available this frame */
        for (;;) {
            int c = getch_nowait();
            if (c == -1)
                break;

            if (c == 'q' || c == 'Q') {
                quit = true;
                break;
            } else if (c == 'w' || c == 'W') {
                accel = 1;
            } else if (c == 's' || c == 'S') {
                brake = 1;
            } else if (c == 'a' || c == 'A') {
                left = 1;
            } else if (c == 'd' || c == 'D') {
                right = 1;
            }
        }

        float old_x = car.x;
        float old_y = car.y;

        /* Physics update */
        update_car(&car, DT, accel, brake, left, right);

        /* Clamp to playable area (just inside outer walls) */
        if (car.x < 1.0f) car.x = 1.0f;
        if (car.x > (float)(TRACK_W - 2)) car.x = (float)(TRACK_W - 2);
        if (car.y < 1.0f) car.y = 1.0f;
        if (car.y > (float)(TRACK_H - 2)) car.y = (float)(TRACK_H - 2);

        /* Wall collision against '#' tiles */
        {
            int ix = (int)(car.x + 0.5f);
            int iy = (int)(car.y + 0.5f);

            if (ix < 0) ix = 0;
            if (ix >= TRACK_W) ix = TRACK_W - 1;
            if (iy < 0) iy = 0;
            if (iy >= TRACK_H) iy = TRACK_H - 1;

            if (track[iy][ix] == '#') {
                /* Simple bounce: step back and lose speed */
                car.x = old_x;
                car.y = old_y;
                car.speed *= -0.3f;
            }
        }

        /* Lap detection: crossing finish line from left to right within band */
        if (!race_finished &&
            old_x < (float)FINISH_X &&
            car.x >= (float)FINISH_X &&
            car.y >= (float)FINISH_Y1 &&
            car.y <= (float)FINISH_Y2 &&
            laps_done < MAX_LAPS) {

            float this_lap = total_time - last_lap_time;
            lap_times[laps_done] = this_lap;
            last_lap_time = total_time;
            laps_done++;

            if (laps_done >= MAX_LAPS)
                race_finished = true;
        }

        /* Draw frame */
        draw(&car, laps_done, total_time);

        /* Frame pacing */
        sleep_ms(FRAME_MS);
    }

#ifndef _WIN32
    disable_raw_mode();
#endif
    show_cursor();

    printf("\n");
    if (race_finished) {
        printf("Race finished in %.2f seconds!\n", total_time);
        for (int i = 0; i < laps_done; ++i) {
            printf("  Lap %d: %.2f s\n", i + 1, lap_times[i]);
        }
    } else if (quit) {
        printf("Race aborted.\n");
    }

    return 0;
}
