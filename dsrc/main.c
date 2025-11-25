/*
 * ASCII Super-Sprint-like racer
 *
 * - Single-file C program
 * - No external libraries beyond standard C + POSIX
 * - Uses ANSI escape codes in a terminal
 *
 * Compile (Linux/macOS, POSIX terminal):
 *   gcc -std=c99 -O2 supersprint_ascii.c -o supersprint_ascii -lm
 *
 * Controls:
 *   W / Up arrow    : accelerate
 *   S / Down arrow  : brake / reverse
 *   A / Left arrow  : turn left
 *   D / Right arrow : turn right
 *   Q               : quit
 *
 * Note: This is an original implementation inspired by
 * classic top-down arcade racers, not an exact port.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <string.h>

/* ---------- Constants ---------- */

#define TRACK_W   60
#define TRACK_H   22

#define NUM_CARS  4
#define MAX_LAPS  3

#define FPS       30
#define DT        (1.0f / (float)FPS)

/* simple PI constants (avoid relying on M_PI definition) */
#define PI        3.14159265358979323846f
#define TWO_PI    (2.0f * PI)

/* ---------- Types ---------- */

typedef struct {
    float x, y;
} Waypoint;

typedef struct {
    float x, y;        /* position in track cells (float) */
    float angle;       /* heading (radians)               */
    float speed;       /* scalar speed (cells / second)   */
    float maxSpeed;
    float accel;
    float turnRate;
    int   laps;
    int   onFinish;    /* 1 if currently over finish line */
    int   targetWp;    /* AI: current waypoint index      */
    int   isPlayer;
    char  symbol;      /* display character               */
} Car;

typedef struct {
    int quit;
    int up;
    int down;
    int left;
    int right;
} InputState;

/* ---------- Globals ---------- */

static char track[TRACK_H][TRACK_W + 1];

static int g_top    = 0;
static int g_bottom = 0;
static int g_left   = 0;
static int g_right  = 0;
static int g_finishX = 0;

#define MAX_WAYPOINTS 16
static Waypoint waypoints[MAX_WAYPOINTS];
static int waypointCount = 0;

static Car cars[NUM_CARS];

static struct termios orig_termios;

/* ---------- Utility / Terminal handling ---------- */

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    /* show cursor */
    printf("\x1b[?25h");
    fflush(stdout);
}

static void handle_sigint(int sig)
{
    (void)sig;
    /* Ensure cleanup */
    restore_terminal();
    _exit(1);
}

static void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    atexit(restore_terminal);
    signal(SIGINT, handle_sigint);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) flags = 0;
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    /* hide cursor and clear screen */
    printf("\x1b[?25l");
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

static float wrap_angle(float a)
{
    while (a > PI)    a -= TWO_PI;
    while (a < -PI)   a += TWO_PI;
    return a;
}

/* ---------- Track / World ---------- */

static int is_driveable(float x, float y)
{
    int ix = (int)(x + 0.5f);
    int iy = (int)(y + 0.5f);
    if (iy < 0 || iy >= TRACK_H || ix < 0 || ix >= TRACK_W)
        return 0;
    char t = track[iy][ix];
    return (t == '.' || t == 'F');
}

static void init_track(void)
{
    /* Fill border with '#' and interior with spaces */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            track[y][x] = '#';
        }
        track[y][TRACK_W] = '\0';
    }

    for (int y = 1; y < TRACK_H - 1; ++y) {
        for (int x = 1; x < TRACK_W - 1; ++x) {
            track[y][x] = ' ';
        }
    }

    /* Define a simple rectangular circuit: a 2-cell-wide loop */

    g_top    = 2;
    g_bottom = TRACK_H - 3;
    g_left   = 2;
    g_right  = TRACK_W - 3;

    /* Top band */
    for (int y = g_top; y <= g_top + 1; ++y) {
        for (int x = g_left; x <= g_right; ++x) {
            track[y][x] = '.';
        }
    }
    /* Bottom band */
    for (int y = g_bottom - 1; y <= g_bottom; ++y) {
        for (int x = g_left; x <= g_right; ++x) {
            track[y][x] = '.';
        }
    }
    /* Left band */
    for (int x = g_left; x <= g_left + 1; ++x) {
        for (int y = g_top; y <= g_bottom; ++y) {
            track[y][x] = '.';
        }
    }
    /* Right band */
    for (int x = g_right - 1; x <= g_right; ++x) {
        for (int y = g_top; y <= g_bottom; ++y) {
            track[y][x] = '.';
        }
    }

    /* Finish line: vertical line across bottom segment (2 cells high) */
    g_finishX = (g_left + g_right) / 2;
    for (int y = g_bottom - 1; y <= g_bottom; ++y) {
        track[y][g_finishX] = 'F';
    }

    /* Set up waypoints for AI (counter-clockwise loop) */
    waypointCount = 4;
    waypoints[0].x = (float)(g_left + 3);
    waypoints[0].y = (float)(g_bottom - 1);

    waypoints[1].x = (float)(g_left + 3);
    waypoints[1].y = (float)(g_top + 1);

    waypoints[2].x = (float)(g_right - 2);
    waypoints[2].y = (float)(g_top + 1);

    waypoints[3].x = (float)(g_right - 2);
    waypoints[3].y = (float)(g_bottom - 1);
}

static void init_cars(void)
{
    float startY = (float)g_bottom - 0.5f;
    /* Start cars on the bottom straight, near the finish */
    for (int i = 0; i < NUM_CARS; ++i) {
        Car *c = &cars[i];
        c->isPlayer = (i == 0);
        c->symbol   = c->isPlayer ? 'P' : ('1' + i);

        /* Stagger starting positions horizontally */
        float offset = (float)((NUM_CARS - 1 - i) * 3);
        c->x = (float)g_finishX + offset;
        if (c->x > (float)(g_right - 2))
            c->x = (float)(g_right - 2);
        c->y = startY;

        c->angle     = PI;   /* face left */
        c->speed     = 0.0f;
        c->maxSpeed  = 18.0f;
        c->accel     = 25.0f;
        c->turnRate  = 2.5f;
        c->laps      = 0;
        c->onFinish  = 0;
        c->targetWp  = 0;
    }
}

/* ---------- Input ---------- */

static void read_input(InputState *in)
{
    memset(in, 0, sizeof(*in));
    unsigned char c;

    for (;;) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == 'q' || c == 'Q') {
            in->quit = 1;
        } else if (c == 'w' || c == 'W') {
            in->up = 1;
        } else if (c == 's' || c == 'S') {
            in->down = 1;
        } else if (c == 'a' || c == 'A') {
            in->left = 1;
        } else if (c == 'd' || c == 'D') {
            in->right = 1;
        } else if (c == 27) {
            /* Possibly an arrow key: ESC [ A/B/C/D */
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) break;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) break;
            if (seq[0] == '[') {
                if (seq[1] == 'A')       in->up    = 1; /* Up */
                else if (seq[1] == 'B')  in->down  = 1; /* Down */
                else if (seq[1] == 'C')  in->right = 1; /* Right */
                else if (seq[1] == 'D')  in->left  = 1; /* Left */
            }
        }
    }
}

/* ---------- Game Logic ---------- */

static void update_lap(Car *c)
{
    int ix = (int)(c->x + 0.5f);
    int iy = (int)(c->y + 0.5f);
    if (iy < 0 || iy >= TRACK_H || ix < 0 || ix >= TRACK_W) {
        c->onFinish = 0;
        return;
    }
    char t = track[iy][ix];

    if (t == 'F') {
        if (!c->onFinish) {
            c->laps += 1;
            c->onFinish = 1;
        }
    } else {
        c->onFinish = 0;
    }
}

static void update_ai(Car *c, float dt)
{
    if (waypointCount <= 0) return;

    Waypoint *wp = &waypoints[c->targetWp];
    float dx = wp->x - c->x;
    float dy = wp->y - c->y;

    float targetAngle = atan2f(dy, dx);
    float diff = wrap_angle(targetAngle - c->angle);

    float maxTurn = c->turnRate * dt;

    if (diff > 0.05f) {
        if (diff > maxTurn) diff = maxTurn;
        c->angle += diff;
    } else if (diff < -0.05f) {
        if (diff < -maxTurn) diff = -maxTurn;
        c->angle += diff;
    }

    float dist2 = dx*dx + dy*dy;
    if (dist2 < 4.0f) {
        c->targetWp = (c->targetWp + 1) % waypointCount;
    }

    float absDiff = fabsf(diff);
    float throttle;
    if (absDiff < 0.4f)
        throttle = 1.0f;
    else if (absDiff < 1.0f)
        throttle = 0.6f;
    else
        throttle = 0.3f;

    if (c->speed < c->maxSpeed) {
        c->speed += c->accel * throttle * dt;
    }
}

static void update_world(float dt, const InputState *in, int *winnerIndex)
{
    for (int i = 0; i < NUM_CARS; ++i) {
        Car *c = &cars[i];

        /* Steering / throttle */
        if (c->isPlayer) {
            float steer = 0.0f;
            if (in->left)  steer -= 1.0f;
            if (in->right) steer += 1.0f;
            c->angle += steer * c->turnRate * dt;
            c->angle = wrap_angle(c->angle);

            float throttle = 0.0f;
            if (in->up)   throttle += 1.0f;
            if (in->down) throttle -= 0.7f; /* braking */
            c->speed += c->accel * throttle * dt;
        } else {
            update_ai(c, dt);
        }

        /* Clamp speed */
        if (c->speed > c->maxSpeed)
            c->speed = c->maxSpeed;
        if (c->speed < -c->maxSpeed * 0.5f)
            c->speed = -c->maxSpeed * 0.5f;

        /* Simple friction */
        const float drag = 5.0f;
        if (c->speed > 0.0f) {
            c->speed -= drag * dt;
            if (c->speed < 0.0f) c->speed = 0.0f;
        } else if (c->speed < 0.0f) {
            c->speed += drag * dt;
            if (c->speed > 0.0f) c->speed = 0.0f;
        }

        /* Move and handle wall collisions */
        float nx = c->x + cosf(c->angle) * c->speed * dt;
        float ny = c->y + sinf(c->angle) * c->speed * dt;

        if (is_driveable(nx, ny)) {
            c->x = nx;
            c->y = ny;
        } else {
            /* bounce and lose speed on wall hit */
            c->speed *= -0.3f;
        }

        update_lap(c);

        if (c->laps >= MAX_LAPS && *winnerIndex < 0) {
            *winnerIndex = i;
        }
    }
}

/* ---------- Rendering ---------- */

static void draw(const Car *cars, int winnerIndex)
{
    /* Move cursor to top-left */
    printf("\x1b[H");

    char buf[TRACK_H][TRACK_W + 1];

    for (int y = 0; y < TRACK_H; ++y) {
        memcpy(buf[y], track[y], TRACK_W + 1);
    }

    /* Overlay cars */
    for (int i = 0; i < NUM_CARS; ++i) {
        const Car *c = &cars[i];
        int ix = (int)(c->x + 0.5f);
        int iy = (int)(c->y + 0.5f);
        if (ix >= 0 && ix < TRACK_W && iy >= 0 && iy < TRACK_H) {
            buf[iy][ix] = c->symbol;
        }
    }

    /* Print track */
    for (int y = 0; y < TRACK_H; ++y) {
        fwrite(buf[y], 1, TRACK_W, stdout);
        fputc('\n', stdout);
    }

    /* HUD */
    printf("Controls: W/S or Up/Down = accel/brake, A/D or Left/Right = steer, Q = quit\n");

    for (int i = 0; i < NUM_CARS; ++i) {
        const Car *c = &cars[i];
        printf("%c  laps: %d/%d  speed: %5.1f\n",
               c->symbol, c->laps, MAX_LAPS, c->speed);
    }

    if (winnerIndex >= 0) {
        const Car *w = &cars[winnerIndex];
        if (w->isPlayer) {
            printf("\nYou win! Press Q to quit.\n");
        } else {
            printf("\nCar %c wins! Press Q to quit.\n", w->symbol);
        }
    } else {
        printf("\nRace in progress...\n");
    }

    fflush(stdout);
}

/* ---------- Main ---------- */

int main(void)
{
    enable_raw_mode();

    init_track();
    init_cars();

    int winnerIndex = -1;
    int running = 1;

    while (running) {
        InputState in;
        read_input(&in);

        if (in.quit) {
            running = 0;
            break;
        }

        if (winnerIndex < 0) {
            update_world(DT, &in, &winnerIndex);
        }

        draw(cars, winnerIndex);

        /* Simple fixed timestep */
        usleep((useconds_t)(DT * 1000000.0f));
    }

    return 0;
}
