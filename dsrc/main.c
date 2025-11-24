/*
 * Simple ASCII Figure-8 Racing Game
 * ---------------------------------
 * - Terminal-based top-down "slot car" style racer.
 * - Player controls speed along a fixed figure-8 track.
 * - Enemies run at fixed speeds.
 * - Lap counter, timer, victory/defeat screen.
 *
 * Controls:
 *   W / w  - accelerate
 *   S / s  - brake
 *   Q / q  - quit
 *
 * Build (POSIX):
 *   gcc -std=c99 -Wall -O2 racer.c -lm -o racer
 *
 * This program uses:
 *   - termios for raw keyboard input (no enter key).
 *   - ANSI escape codes for screen control.
 *   - math.h for sin/cos to generate a figure-8 path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <string.h>

/* ---------------- Configuration ---------------- */

#define TRACK_WIDTH   60
#define TRACK_HEIGHT  20

#define PATH_MAX      2048
#define PATH_SAMPLES  1024

#define TOTAL_LAPS    3

#define NUM_ENEMIES   2

#define PLAYER_ACCEL_STEP  2.0    /* change in speed per key press */
#define PLAYER_MAX_SPEED   45.0   /* path units per second */
#define PLAYER_START_SPEED 15.0

#define ENEMY_SPEED_1      30.0
#define ENEMY_SPEED_2      27.0

#define FRAME_SLEEP_US     15000  /* microseconds between frames (~66 FPS) */

/* ---------------- Terminal handling ---------------- */

static struct termios orig_termios;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    /* Show cursor */
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(restore_terminal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); /* no echo, no canonical mode */
    raw.c_cc[VMIN] = 0;              /* non-blocking reads */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    /* Hide cursor */
    write(STDOUT_FILENO, "\x1b[?25l", 6);
}

static void handle_signal(int sig) {
    (void)sig;
    restore_terminal();
    _exit(1);
}

/* ---------------- Time helpers ---------------- */

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ---------------- Track / path ---------------- */

static int pathLen = 0;
static int pathX[PATH_MAX];
static int pathY[PATH_MAX];

static char baseTrack[TRACK_HEIGHT][TRACK_WIDTH + 1];

static void build_track_path(void) {
    /* Generate a lemniscate (figure-8) centerline using trig */
    const double PI = 3.14159265358979323846;
    int cx = TRACK_WIDTH / 2;
    int cy = TRACK_HEIGHT / 2;
    double sx = (TRACK_WIDTH - 4) / 2.0;    /* horizontal radius */
    double sy = (TRACK_HEIGHT - 4);         /* vertical radius (note 0.5 factor later) */

    int lastX = -1000, lastY = -1000;
    pathLen = 0;

    for (int i = 0; i < PATH_SAMPLES && pathLen < PATH_MAX; i++) {
        double t = 2.0 * PI * (double)i / (double)PATH_SAMPLES;
        double s = sin(t);
        double yterm = 0.5 * sin(2.0 * t); /* sin(t)*cos(t) = 0.5*sin(2t) */

        int x = cx + (int)lrint(sx * s);
        int y = cy + (int)lrint(sy * yterm);

        if (x < 1) x = 1;
        if (x > TRACK_WIDTH - 2) x = TRACK_WIDTH - 2;
        if (y < 1) y = 1;
        if (y > TRACK_HEIGHT - 2) y = TRACK_HEIGHT - 2;

        if (x == lastX && y == lastY) {
            continue; /* skip duplicate points */
        }

        pathX[pathLen] = x;
        pathY[pathLen] = y;
        lastX = x;
        lastY = y;
        pathLen++;
    }

    /* Ensure closure: last point same as first */
    if (pathLen > 0) {
        if (pathX[pathLen - 1] != pathX[0] || pathY[pathLen - 1] != pathY[0]) {
            if (pathLen < PATH_MAX) {
                pathX[pathLen] = pathX[0];
                pathY[pathLen] = pathY[0];
                pathLen++;
            }
        }
    }

    if (pathLen < 10) {
        fprintf(stderr, "Path too short, something went wrong.\n");
        exit(1);
    }
}

static void build_base_track(void) {
    /* Clear with spaces and border with '#', then place path as '.' and start line 'S'. */
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        for (int x = 0; x < TRACK_WIDTH; x++) {
            if (y == 0 || y == TRACK_HEIGHT - 1 || x == 0 || x == TRACK_WIDTH - 1) {
                baseTrack[y][x] = '#';
            } else {
                baseTrack[y][x] = ' ';
            }
        }
        baseTrack[y][TRACK_WIDTH] = '\0';
    }

    /* Mark the path */
    for (int i = 0; i < pathLen; i++) {
        int x = pathX[i];
        int y = pathY[i];
        if (x >= 1 && x < TRACK_WIDTH - 1 &&
            y >= 1 && y < TRACK_HEIGHT - 1) {
            baseTrack[y][x] = '.';
        }
    }

    /* Start/finish line at path[0] */
    int sx = pathX[0];
    int sy = pathY[0];
    baseTrack[sy][sx] = 'S';
}

/* ---------------- Cars / race state ---------------- */

typedef struct {
    double pos;        /* position along path (index units, wraps at pathLen) */
    double speed;      /* units per second along path */
    int    laps;       /* completed laps */
    int    finished;   /* 1 if race finished for this car */
    double finishTime; /* time at which it finished */
    char   symbol;     /* character drawn on track */
    const char *name;  /* for summary */
} Car;

static Car player;
static Car enemies[NUM_ENEMIES];

static void init_cars(void) {
    player.pos = 0.0;
    player.speed = PLAYER_START_SPEED;
    player.laps = 0;
    player.finished = 0;
    player.finishTime = 0.0;
    player.symbol = 'P';
    player.name = "You";

    /* Spread enemies around the track */
    double offset = (double)pathLen / (double)(NUM_ENEMIES + 1);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].pos = offset * (i + 1);
        enemies[i].laps = 0;
        enemies[i].finished = 0;
        enemies[i].finishTime = 0.0;
        enemies[i].symbol = 'A' + i;
        enemies[i].name = (i == 0) ? "CPU A" : "CPU B";
        enemies[i].speed = (i == 0) ? ENEMY_SPEED_1 : ENEMY_SPEED_2;
    }
}

/* Move a car along the path based on its speed and dt */
static void advance_car(Car *c, double dt) {
    if (c->finished) return;

    c->pos += c->speed * dt;
    while (c->pos >= (double)pathLen) {
        c->pos -= (double)pathLen;
        c->laps++;
    }
    while (c->pos < 0.0) {
        c->pos += (double)pathLen;
        if (c->laps > 0) c->laps--;
    }
}

/* Simple rank based on laps + fraction of current lap */
static int compute_player_position(void) {
    double playerProg = (double)player.laps + player.pos / (double)pathLen;
    int pos = 1;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        double eProg = (double)enemies[i].laps + enemies[i].pos / (double)pathLen;
        if (eProg > playerProg) {
            pos++;
        }
    }
    return pos;
}

/* ---------------- Input handling ---------------- */

static int read_last_key(int *quit_flag) {
    char c;
    int last = 0;

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 'q' || c == 'Q') {
            *quit_flag = 1;
            return 0;
        }
        last = (unsigned char)c;
    }
    return last;
}

/* ---------------- Rendering ---------------- */

static void render(double elapsedTime) {
    /* Clear screen and move cursor home */
    write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);

    int minutes = (int)(elapsedTime / 60.0);
    int seconds = (int)elapsedTime % 60;
    int lapDisplay = player.laps + 1;
    if (lapDisplay > TOTAL_LAPS) lapDisplay = TOTAL_LAPS;

    int position = compute_player_position();

    printf("ASCII Figure-8 Racer\n");
    printf("Controls: W/S = accelerate/brake, Q = quit\n");
    printf("Lap: %d/%d  Time: %02d:%02d  Speed: %5.1f  Position: %d/%d\n",
           lapDisplay, TOTAL_LAPS, minutes, seconds,
           player.speed, position, 1 + NUM_ENEMIES);

    /* Prepare a working buffer from baseTrack */
    char buf[TRACK_HEIGHT][TRACK_WIDTH + 1];
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        memcpy(buf[y], baseTrack[y], TRACK_WIDTH + 1);
    }

    /* Draw cars (player then enemies) */
    int pi = (int)player.pos % pathLen;
    int px = pathX[pi];
    int py = pathY[pi];
    buf[py][px] = player.symbol;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        int ei = (int)enemies[i].pos % pathLen;
        int ex = pathX[ei];
        int ey = pathY[ei];
        buf[ey][ex] = enemies[i].symbol;
    }

    /* Print track */
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        printf("%s\n", buf[y]);
    }

    fflush(stdout);
}

/* ---------------- Main game loop ---------------- */

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    enable_raw_mode();

    build_track_path();
    build_base_track();
    init_cars();

    double startTime = get_time_sec();
    double lastTime  = startTime;

    int raceFinished = 0;
    int winnerIndex = -1;  /* 0: player, 1..NUM_ENEMIES: enemies */
    double finishTime = 0.0;

    while (!raceFinished) {
        int quit_flag = 0;
        int key = read_last_key(&quit_flag);
        if (quit_flag) {
            restore_terminal();
            printf("\nRace aborted.\n");
            return 0;
        }

        double now = get_time_sec();
        double dt = now - lastTime;
        if (dt > 0.1) dt = 0.1; /* avoid huge jumps if paused */
        lastTime = now;

        /* Handle player input: speed changes are discrete per keypress */
        if (key == 'w' || key == 'W') {
            player.speed += PLAYER_ACCEL_STEP;
            if (player.speed > PLAYER_MAX_SPEED) {
                player.speed = PLAYER_MAX_SPEED;
            }
        } else if (key == 's' || key == 'S') {
            player.speed -= PLAYER_ACCEL_STEP;
            if (player.speed < 0.0) {
                player.speed = 0.0;
            }
        }

        /* Update cars */
        advance_car(&player, dt);
        for (int i = 0; i < NUM_ENEMIES; i++) {
            advance_car(&enemies[i], dt);
        }

        /* Check for finish */
        if (player.laps >= TOTAL_LAPS && !player.finished) {
            player.finished = 1;
            player.finishTime = now - startTime;
            raceFinished = 1;
            winnerIndex = 0;
            finishTime = player.finishTime;
        }

        for (int i = 0; i < NUM_ENEMIES && !raceFinished; i++) {
            if (enemies[i].laps >= TOTAL_LAPS && !enemies[i].finished) {
                enemies[i].finished = 1;
                enemies[i].finishTime = now - startTime;
                raceFinished = 1;
                winnerIndex = 1 + i;
                finishTime = enemies[i].finishTime;
            }
        }

        /* Render current frame */
        render(now - startTime);

        if (raceFinished) {
            break;
        }

        usleep(FRAME_SLEEP_US);
    }

    restore_terminal();

    /* Final screen */
    printf("\n=== Race Over ===\n\n");
    if (winnerIndex == 0) {
        printf("You win!\n");
        printf("Your time: %.2f seconds\n", finishTime);
    } else if (winnerIndex > 0) {
        int ei = winnerIndex - 1;
        printf("You lose. Winner: %s\n", enemies[ei].name);
        printf("Winning time: %.2f seconds\n", finishTime);
        if (player.laps >= TOTAL_LAPS) {
            printf("Your time: %.2f seconds\n", player.finishTime);
        } else {
            printf("You did not complete all %d laps.\n", TOTAL_LAPS);
        }
    } else {
        printf("Race ended unexpectedly.\n");
    }

    printf("\nFinal standings (by progress):\n");

    /* Compute final progress for ranking */
    double prog[1 + NUM_ENEMIES];
    const char *names[1 + NUM_ENEMIES];
    prog[0] = (double)player.laps + player.pos / (double)pathLen;
    names[0] = player.name;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        prog[1 + i] = (double)enemies[i].laps +
                      enemies[i].pos / (double)pathLen;
        names[1 + i] = enemies[i].name;
    }

    /* Simple bubble sort for 3 entries */
    int idx[1 + NUM_ENEMIES] = {0, 1, 2};
    int totalCars = 1 + NUM_ENEMIES;
    for (int i = 0; i < totalCars - 1; i++) {
        for (int j = 0; j < totalCars - 1 - i; j++) {
            if (prog[idx[j]] < prog[idx[j + 1]]) {
                int tmp = idx[j];
                idx[j] = idx[j + 1];
                idx[j + 1] = tmp;
            }
        }
    }

    for (int rank = 0; rank < totalCars; rank++) {
        int id = idx[rank];
        printf("%d. %s (progress: %.3f laps)\n",
               rank + 1, names[id], prog[id]);
    }

    printf("\nThanks for playing!\n");
    return 0;
}
