#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

/* --------------------------- Configuration --------------------------- */

#define TRACK_W 60
#define TRACK_H 24

#define NUM_AI       2
#define TOTAL_CARS   (1 + NUM_AI)
#define MAX_LAPS     3

#define PLAYER_MAX_SPEED 22.0f
#define AI_MAX_SPEED     20.0f

#define ACCEL       40.0f
#define BRAKE       60.0f
#define FRICTION    12.0f

#define TURN_STEP   0.12f      /* Player turn per keypress (radians) */
#define AI_TURN_RATE 3.5f      /* AI max turn rate (radians/sec) */

#define DT          (1.0f / 30.0f)  /* Fixed timestep ~30 FPS */
#define FRAME_MS    33

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Track rectangle parameters (used by generator & waypoints) */
static const int outer_x1 = 3;
static const int outer_x2 = 56;
static const int outer_y1 = 2;
static const int outer_y2 = 20;

static const int inner_x1 = 20;
static const int inner_x2 = 39;
static const int inner_y1 = 7;
static const int inner_y2 = 15;

/* ---------------------------- Data Types ----------------------------- */

typedef struct {
    float x, y;
    float angle;
    float speed;
    float max_speed;
    char symbol;
    int   laps;
    int   finished;
    float prev_x, prev_y;
} Car;

typedef struct {
    float x, y;
} Waypoint;

/* -------------------------- Global State ----------------------------- */

static char track[TRACK_H][TRACK_W];
static Car cars[TOTAL_CARS];

#define NUM_WAYPOINTS 8
static Waypoint waypoints[NUM_WAYPOINTS];
static int ai_target_wp[TOTAL_CARS];

static int start_x;
static int start_y1, start_y2;

static struct termios orig_termios;

/* -------------------------- Declarations ----------------------------- */

static void enable_raw_mode(void);
static void disable_raw_mode(void);
static void cleanup(void);
static int  kbhit(void);
static int  getch_nonblock(void);
static void sleep_ms(int ms);

static void init_track(void);
static void init_waypoints(void);
static void init_cars(void);

static void handle_input(Car *player, int *quit, float dt);
static void apply_friction(Car *car, float dt);
static void move_car(Car *car, float dt);
static void update_lap(Car *car);
static float wrap_angle(float a);
static void update_player(Car *car, float dt);
static void update_ai(Car *car, int idx, float dt);
static void render(float total_time);

/* ------------------------- Terminal Handling ------------------------- */

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void cleanup(void) {
    disable_raw_mode();
    /* Show cursor and reset attributes */
    printf("\x1b[?25h\x1b[0m\n");
    fflush(stdout);
}

static int kbhit(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int getch_nonblock(void) {
    unsigned char c;
    if (!kbhit())
        return -1;
    if (read(STDIN_FILENO, &c, 1) < 1)
        return -1;
    return c;
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* --------------------------- Track Setup ----------------------------- */

static void init_track(void) {
    /* Clear */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            track[y][x] = ' ';
        }
    }

    /* Create road corridors forming a rectangular ring */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            int road = 0;

            /* Top horizontal segment */
            if (y >= outer_y1 + 1 && y <= inner_y1 - 1 &&
                x >= outer_x1 + 1 && x <= outer_x2 - 1) {
                road = 1;
            }

            /* Bottom horizontal segment */
            if (y >= inner_y2 + 1 && y <= outer_y2 - 1 &&
                x >= outer_x1 + 1 && x <= outer_x2 - 1) {
                road = 1;
            }

            /* Left vertical segment */
            if (x >= outer_x1 + 1 && x <= inner_x1 - 1 &&
                y >= inner_y1 && y <= inner_y2) {
                road = 1;
            }

            /* Right vertical segment */
            if (x >= inner_x2 + 1 && x <= outer_x2 - 1 &&
                y >= inner_y1 && y <= inner_y2) {
                road = 1;
            }

            if (road) {
                track[y][x] = '.';
            }
        }
    }

    /* Add walls (#) around road (cells adjacent to '.' in 4 directions) */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            if (track[y][x] == '.')
                continue;
            int near_road = 0;
            if (y > 0 && track[y - 1][x] == '.') near_road = 1;
            if (y < TRACK_H - 1 && track[y + 1][x] == '.') near_road = 1;
            if (x > 0 && track[y][x - 1] == '.') near_road = 1;
            if (x < TRACK_W - 1 && track[y][x + 1] == '.') near_road = 1;
            if (near_road)
                track[y][x] = '#';
        }
    }

    /* Start/finish line: vertical '=' on the top straight */
    start_x  = (outer_x1 + outer_x2) / 2;
    start_y1 = outer_y1 + 1;
    start_y2 = inner_y1 - 1;

    for (int y = start_y1; y <= start_y2; ++y) {
        if (track[y][start_x] == '.')
            track[y][start_x] = '=';
    }
}

static void init_waypoints(void) {
    float cx_left   = (outer_x1 + inner_x1) / 2.0f;
    float cx_right  = (inner_x2 + outer_x2) / 2.0f;
    float cy_top    = (outer_y1 + inner_y1) / 2.0f;
    float cy_bottom = (inner_y2 + outer_y2) / 2.0f;
    float cy_mid    = (inner_y1 + inner_y2) / 2.0f;
    float sx        = (float)start_x + 2.0f;

    /* Clockwise loop around the track */
    waypoints[0].x = sx;        waypoints[0].y = cy_top;
    waypoints[1].x = cx_right;  waypoints[1].y = cy_top;
    waypoints[2].x = cx_right;  waypoints[2].y = cy_mid;
    waypoints[3].x = cx_right;  waypoints[3].y = cy_bottom;
    waypoints[4].x = sx;        waypoints[4].y = cy_bottom;
    waypoints[5].x = cx_left;   waypoints[5].y = cy_bottom;
    waypoints[6].x = cx_left;   waypoints[6].y = cy_mid;
    waypoints[7].x = cx_left;   waypoints[7].y = cy_top;
}

/* ---------------------------- Cars Setup ----------------------------- */

static void init_cars(void) {
    float cy_top = (outer_y1 + inner_y1) / 2.0f;

    for (int i = 0; i < TOTAL_CARS; ++i) {
        cars[i].x = (float)start_x - 4.0f - (float)i;
        cars[i].y = cy_top;
        cars[i].angle = 0.0f; /* facing right */
        cars[i].speed = 0.0f;
        cars[i].laps  = 0;
        cars[i].finished = 0;
        cars[i].prev_x = cars[i].x;
        cars[i].prev_y = cars[i].y;
        cars[i].max_speed = (i == 0) ? PLAYER_MAX_SPEED : AI_MAX_SPEED;
        cars[i].symbol = (i == 0) ? 'P' : ('1' + (char)(i - 1));
        ai_target_wp[i] = 0;
    }
}

/* ------------------------------ Helpers ------------------------------ */

static int is_drivable_cell(int tx, int ty) {
    if (tx < 0 || tx >= TRACK_W || ty < 0 || ty >= TRACK_H)
        return 0;
    char c = track[ty][tx];
    return (c == '.' || c == '=');
}

static float wrap_angle(float a) {
    while (a <= -M_PI) a += 2.0f * (float)M_PI;
    while (a >   M_PI) a -= 2.0f * (float)M_PI;
    return a;
}

/* ---------------------------- Game Logic ----------------------------- */

static void handle_input(Car *player, int *quit, float dt) {
    int c;
    while ((c = getch_nonblock()) != -1) {
        if (c == 'q' || c == 'Q') {
            *quit = 1;
        } else if (c == 'w' || c == 'W') {
            player->speed += ACCEL * dt;
        } else if (c == 's' || c == 'S') {
            player->speed -= BRAKE * dt;
        } else if (c == 'a' || c == 'A') {
            player->angle -= TURN_STEP;
        } else if (c == 'd' || c == 'D') {
            player->angle += TURN_STEP;
        }
    }
}

static void apply_friction(Car *car, float dt) {
    if (car->speed > 0.0f) {
        car->speed -= FRICTION * dt;
        if (car->speed < 0.0f) car->speed = 0.0f;
    } else if (car->speed < 0.0f) {
        car->speed += FRICTION * dt;
        if (car->speed > 0.0f) car->speed = 0.0f;
    }
}

static void move_car(Car *car, float dt) {
    if (fabsf(car->speed) < 0.01f)
        return;

    float old_x = car->x;
    float old_y = car->y;

    car->x += cosf(car->angle) * car->speed * dt;
    car->y += sinf(car->angle) * car->speed * dt;

    int tx = (int)(car->x + 0.5f);
    int ty = (int)(car->y + 0.5f);

    if (!is_drivable_cell(tx, ty)) {
        /* Hit a wall or off-track: revert and bounce a bit */
        car->x = old_x;
        car->y = old_y;
        car->speed *= -0.2f;
    }
}

static void update_lap(Car *car) {
    if (car->finished)
        return;

    float y = car->y;
    int within = (y >= (float)start_y1 && y <= (float)start_y2);

    if (car->prev_x < (float)start_x && car->x >= (float)start_x && within) {
        car->laps++;
        if (car->laps >= MAX_LAPS) {
            car->finished = 1;
        }
    }

    car->prev_x = car->x;
    car->prev_y = car->y;
}

static void update_player(Car *car, float dt) {
    apply_friction(car, dt);

    if (car->speed > car->max_speed)
        car->speed = car->max_speed;
    if (car->speed < -car->max_speed * 0.5f)
        car->speed = -car->max_speed * 0.5f;

    move_car(car, dt);
    update_lap(car);
}

static void update_ai(Car *car, int idx, float dt) {
    if (!car->finished) {
        Waypoint *wp = &waypoints[ai_target_wp[idx]];
        float dx = wp->x - car->x;
        float dy = wp->y - car->y;
        float dist = sqrtf(dx * dx + dy * dy);

        float target_angle = atan2f(dy, dx);
        float diff = wrap_angle(target_angle - car->angle);

        float max_turn = AI_TURN_RATE * dt;
        if (diff > max_turn) diff = max_turn;
        if (diff < -max_turn) diff = -max_turn;
        car->angle += diff;

        if (dist < 2.0f) {
            ai_target_wp[idx] = (ai_target_wp[idx] + 1) % NUM_WAYPOINTS;
        }

        float target_speed = car->max_speed * 0.85f;
        if (fabsf(diff) > 0.8f) {
            target_speed *= 0.6f; /* Slow in sharp turns */
        }

        if (car->speed < target_speed)
            car->speed += ACCEL * dt;
        else
            car->speed -= ACCEL * dt * 0.5f;
    }

    apply_friction(car, dt);

    if (car->speed > car->max_speed)
        car->speed = car->max_speed;
    if (car->speed < -car->max_speed * 0.5f)
        car->speed = -car->max_speed * 0.5f;

    move_car(car, dt);
    update_lap(car);
}

/* ------------------------------ Rendering ---------------------------- */

static void render(float total_time) {
    char buffer[TRACK_H][TRACK_W + 1];

    /* Copy track */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            buffer[y][x] = track[y][x];
        }
        buffer[y][TRACK_W] = '\0';
    }

    /* Draw AI cars first */
    for (int i = 1; i < TOTAL_CARS; ++i) {
        int tx = (int)(cars[i].x + 0.5f);
        int ty = (int)(cars[i].y + 0.5f);
        if (tx >= 0 && tx < TRACK_W && ty >= 0 && ty < TRACK_H) {
            buffer[ty][tx] = cars[i].symbol;
        }
    }

    /* Draw player last so it's on top */
    {
        int tx = (int)(cars[0].x + 0.5f);
        int ty = (int)(cars[0].y + 0.5f);
        if (tx >= 0 && tx < TRACK_W && ty >= 0 && ty < TRACK_H) {
            buffer[ty][tx] = cars[0].symbol;
        }
    }

    /* Move cursor to top-left and draw */
    printf("\x1b[H");
    for (int y = 0; y < TRACK_H; ++y) {
        printf("%.*s\n", TRACK_W, buffer[y]);
    }

    Car *p = &cars[0];
    printf("Laps: %d / %d   Time: %.1f s   Speed: %.1f   (WASD to drive, Q to quit)\n",
           p->laps, MAX_LAPS, total_time, p->speed);

    printf("AI: 1 laps=%d  2 laps=%d\n",
           cars[1].laps, (TOTAL_CARS > 2) ? cars[2].laps : 0);

    if (p->finished) {
        printf("You finished %d laps! Press Q to quit.\n", MAX_LAPS);
    }

    fflush(stdout);
}

/* -------------------------------- main ------------------------------ */

int main(void) {
    enable_raw_mode();
    atexit(cleanup);

    /* Clear screen and hide cursor */
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);

    init_track();
    init_waypoints();
    init_cars();

    int quit = 0;
    float total_time = 0.0f;

    while (!quit) {
        handle_input(&cars[0], &quit, DT);

        update_player(&cars[0], DT);
        for (int i = 1; i < TOTAL_CARS; ++i) {
            update_ai(&cars[i], i, DT);
        }

        total_time += DT;

        render(total_time);

        /* Optional auto-exit when player finished and some time passed */
        if (cars[0].finished) {
            /* Just keep running until user presses Q */
        }

        sleep_ms(FRAME_MS);
    }

    return 0;
}
