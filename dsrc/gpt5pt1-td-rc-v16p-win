/*
 * ASCII Super Sprint-style top-down racer
 *
 * - Single file
 * - No external dependencies (just standard C + OS APIs)
 * - Runs in a text terminal
 *
 * Controls:
 *   Arrow Up    = accelerate
 *   Arrow Down  = brake / reverse
 *   Arrow Left  = steer left
 *   Arrow Right = steer right
 *   Q           = quit
 *
 * Notes:
 *   - Unix-like: uses termios to set noncanonical input and ANSI escape codes.
 *   - Windows: uses _kbhit/_getch and "cls" to clear the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef _WIN32
#include <unistd.h>     /* usleep, read */
#include <termios.h>    /* terminal raw mode */
#else
#include <conio.h>      /* _kbhit, _getch */
#include <windows.h>    /* Sleep, system("cls") */
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Screen and track sizes */
#define SCREEN_W       80
#define SCREEN_H       24

#define TRACK_W        60
#define TRACK_H        20
#define TRACK_OFFSET_X 10
#define TRACK_OFFSET_Y 3

/* Game parameters */
#define NUM_CARS   4
#define MAX_LAPS   3

/* Physics parameters */
#define DT                 0.05f   /* 50 ms per frame ~ 20 FPS */
#define ACCEL              20.0f
#define BRAKE              30.0f
#define FRICTION           5.0f
#define MAX_FWD_SPEED      12.0f
#define MAX_REV_SPEED     -5.0f
#define TURN_RATE          4.0f    /* rad/s at full steer */
#define COLLISION_DIST     0.7f

typedef struct {
    int up, down, left, right, quit;
} InputState;

typedef struct {
    float x, y;
    float angle;            /* radians */
    float speed;            /* scalar along facing direction */
    int   laps;
    int   onStartTile;
    int   passedCheckpoint;
    int   isPlayer;
    char  symbol;
    int   currentWaypoint;  /* for AI */
} Car;

/* Track grid */
static char track[TRACK_H][TRACK_W];

/* Waypoints for AI to follow the circuit (clockwise-ish) */
#define NUM_WAYPOINTS 8
static const float waypoints[NUM_WAYPOINTS][2] = {
    {55.0f,  4.0f}, /* top-right */
    {30.0f,  2.0f}, /* top-middle */
    { 5.0f,  2.0f}, /* top-left (near checkpoint) */
    { 3.0f, 10.0f}, /* mid-left  */
    { 5.0f, 16.0f}, /* bottom-left */
    {30.0f, 18.0f}, /* bottom-middle */
    {55.0f, 16.0f}, /* bottom-right */
    {57.0f, 10.0f}  /* mid-right (near start/finish) */
};

#ifndef _WIN32
static struct termios orig_termios;
#endif

/* ---------- Platform helpers ---------- */

#ifndef _WIN32
static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    atexit(disable_raw_mode);
}
#endif

static void hide_cursor(void)
{
    /* ANSI code; harmless on most consoles */
    printf("\x1b[?25l");
}

static void show_cursor(void)
{
    printf("\x1b[?25h");
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* ---------- Track and world ---------- */

static void init_track(void)
{
    /* Start with open space */
    for (int y = 0; y < TRACK_H; ++y) {
        for (int x = 0; x < TRACK_W; ++x) {
            track[y][x] = ' ';
        }
    }

    /* Outer walls */
    for (int x = 0; x < TRACK_W; ++x) {
        track[0][x]            = '#';
        track[TRACK_H - 1][x]  = '#';
    }
    for (int y = 0; y < TRACK_H; ++y) {
        track[y][0]            = '#';
        track[y][TRACK_W - 1]  = '#';
    }

    /* Inner rectangular island to make a ring-shaped track */
    int top    = 4;
    int bottom = 15;
    int left   = 12;
    int right  = 48;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (y == top || y == bottom || x == left || x == right)
                track[y][x] = '#';
            else
                track[y][x] = ' ';
        }
    }

    /* Start/finish line: vertical '=' on right side corridor */
    int startX = 55;
    for (int y = 9; y <= 11; ++y) {
        track[y][startX] = '=';
    }

    /* Checkpoint somewhere on the left side */
    int checkpointX = 4;
    int checkpointY = 2;
    track[checkpointY][checkpointX] = '+';
}

/* Helper: is a tile solid (wall)? */
static int is_solid_tile(int tx, int ty)
{
    if (tx < 0 || tx >= TRACK_W || ty < 0 || ty >= TRACK_H)
        return 1; /* treat out of bounds as wall */
    char t = track[ty][tx];
    return (t == '#');
}

/* Get underlying track tile at car position */
static char get_car_tile(const Car *c)
{
    int tx = (int)(c->x + 0.5f);
    int ty = (int)(c->y + 0.5f);
    if (tx < 0 || tx >= TRACK_W || ty < 0 || ty >= TRACK_H)
        return '#';
    return track[ty][tx];
}

/* ---------- Cars and AI ---------- */

static void init_cars(Car cars[NUM_CARS])
{
    for (int i = 0; i < NUM_CARS; ++i) {
        cars[i].x               = 55.0f - (float)((i % 2) * 2); /* small offsets */
        cars[i].y               = 12.0f + (float)(i / 2);
        cars[i].angle           = - (float)M_PI / 2.0f; /* facing "up" */
        cars[i].speed           = 0.0f;
        cars[i].laps            = 0;
        cars[i].onStartTile     = 0;
        cars[i].passedCheckpoint= 0;
        cars[i].isPlayer        = (i == 0);
        cars[i].symbol          = (char)('1' + i);
        cars[i].currentWaypoint = 0;
    }
}

/* Basic car physics update */
static void update_car_physics(Car *c, const InputState *in, float dt)
{
    /* Throttle and brake */
    if (in->up)
        c->speed += ACCEL * dt;
    if (in->down)
        c->speed -= BRAKE * dt;

    /* Friction */
    if (c->speed > 0.0f) {
        c->speed -= FRICTION * dt;
        if (c->speed < 0.0f)
            c->speed = 0.0f;
    } else if (c->speed < 0.0f) {
        c->speed += FRICTION * dt;
        if (c->speed > 0.0f)
            c->speed = 0.0f;
    }

    /* Clamp */
    if (c->speed > MAX_FWD_SPEED)
        c->speed = MAX_FWD_SPEED;
    if (c->speed < MAX_REV_SPEED)
        c->speed = MAX_REV_SPEED;

    /* Steering */
    float steerInput = 0.0f;
    if (in->left)  steerInput -= 1.0f;
    if (in->right) steerInput += 1.0f;

    float speedFactor = 0.5f + 0.5f * (fabsf(c->speed) / MAX_FWD_SPEED);
    if (speedFactor > 1.0f) speedFactor = 1.0f;

    c->angle += steerInput * TURN_RATE * dt * speedFactor;

    /* Normalize angle to [-pi, pi] for numeric stability */
    if (c->angle > (float)M_PI)  c->angle -= 2.0f * (float)M_PI;
    if (c->angle < -(float)M_PI) c->angle += 2.0f * (float)M_PI;

    /* Movement with simple wall collision handling (separately on x and y) */
    float dx = cosf(c->angle) * c->speed * dt;
    float dy = sinf(c->angle) * c->speed * dt;

    float newX = c->x + dx;
    int   tx   = (int)(newX + 0.5f);
    int   ty   = (int)(c->y + 0.5f);
    if (!is_solid_tile(tx, ty)) {
        c->x = newX;
    } else {
        c->speed *= -0.3f; /* mild bounce */
    }

    float newY = c->y + dy;
    tx = (int)(c->x + 0.5f);
    ty = (int)(newY + 0.5f);
    if (!is_solid_tile(tx, ty)) {
        c->y = newY;
    } else {
        c->speed *= -0.3f;
    }
}

/* Car-vs-car simple collision/bumps */
static void handle_car_collisions(Car *cars, int numCars)
{
    const float minDist = COLLISION_DIST;
    const float minDist2 = minDist * minDist;

    for (int i = 0; i < numCars; ++i) {
        for (int j = i + 1; j < numCars; ++j) {
            float dx = cars[j].x - cars[i].x;
            float dy = cars[j].y - cars[i].y;
            float dist2 = dx*dx + dy*dy;
            if (dist2 < minDist2 && dist2 > 1e-4f) {
                float dist = sqrtf(dist2);
                float overlap = 0.5f * (minDist - dist);
                dx /= dist;
                dy /= dist;

                /* Push cars apart */
                cars[i].x -= dx * overlap;
                cars[i].y -= dy * overlap;
                cars[j].x += dx * overlap;
                cars[j].y += dy * overlap;

                /* Exchange some speed (very crude) */
                float tmp = cars[i].speed;
                cars[i].speed = cars[j].speed;
                cars[j].speed = tmp;
            }
        }
    }
}

/* Lap and checkpoint logic */
static void update_lap_and_checkpoint(Car *c)
{
    char tile = get_car_tile(c);

    if (tile == '+') {
        c->passedCheckpoint = 1;
    }

    int onStart = (tile == '=');

    if (tile == '=' && !c->onStartTile && c->passedCheckpoint) {
        c->laps += 1;
        c->passedCheckpoint = 0;
    }

    c->onStartTile = onStart;
}

/* AI control to follow waypoints */
static void compute_ai_input(Car *c, InputState *ai)
{
    memset(ai, 0, sizeof(*ai));

    float targetX = waypoints[c->currentWaypoint][0];
    float targetY = waypoints[c->currentWaypoint][1];

    float dx = targetX - c->x;
    float dy = targetY - c->y;

    float desiredAngle = atan2f(dy, dx);
    float diff = desiredAngle - c->angle;

    /* Normalize diff to [-pi, pi] */
    while (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
    while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;

    /* Decide steering */
    if (diff > 0.1f)
        ai->right = 1;
    else if (diff < -0.1f)
        ai->left = 1;

    /* Throttle / brake based on turning need and current speed */
    float absDiff = fabsf(diff);
    if (absDiff < 0.4f) {
        ai->up = 1;   /* full throttle on straights / mild turns */
    } else if (absDiff < 1.0f) {
        ai->up = 1;   /* still throttle but will be turning */
    } else {
        if (c->speed > 6.0f)
            ai->down = 1;  /* brake for sharp turn at speed */
        else
            ai->up = 1;    /* low-speed: try to keep moving */
    }

    /* Advance to next waypoint when close enough */
    float dist2 = dx*dx + dy*dy;
    if (dist2 < 9.0f) { /* within 3 tiles */
        c->currentWaypoint = (c->currentWaypoint + 1) % NUM_WAYPOINTS;
    }
}

/* ---------- Input ---------- */

static void poll_player_input(InputState *in)
{
    memset(in, 0, sizeof(*in));

#ifdef _WIN32
    while (_kbhit()) {
        int ch = _getch();
        if (ch == 0 || ch == 224) {
            int ch2 = _getch();
            switch (ch2) {
                case 72: in->up    = 1; break; /* up arrow */
                case 80: in->down  = 1; break; /* down */
                case 75: in->left  = 1; break; /* left */
                case 77: in->right = 1; break; /* right */
                default: break;
            }
        } else {
            if (ch == 'q' || ch == 'Q')
                in->quit = 1;
        }
    }
#else
    static int esc_state = 0;
    unsigned char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (esc_state == 0) {
            if (ch == 27) {   /* ESC */
                esc_state = 1;
            } else {
                if (ch == 'q' || ch == 'Q')
                    in->quit = 1;
            }
        } else if (esc_state == 1) {
            if (ch == '[') {
                esc_state = 2;
            } else {
                esc_state = 0;
            }
        } else if (esc_state == 2) {
            switch (ch) {
                case 'A': in->up    = 1; break; /* up */
                case 'B': in->down  = 1; break; /* down */
                case 'C': in->right = 1; break; /* right */
                case 'D': in->left  = 1; break; /* left */
                default: break;
            }
            esc_state = 0;
        }
    }
#endif
}

/* ---------- Rendering ---------- */

static void clear_screen(void)
{
#ifdef _WIN32
    system("cls");
#else
    /* Move cursor home and clear screen */
    printf("\x1b[H\x1b[2J");
#endif
}

static void render(const Car *cars, int numCars, int maxLaps)
{
    clear_screen();

    /* Top HUD */
    printf("ASCII SUPER SPRINT  -  First to %d laps wins\n", maxLaps);
    printf("P1[%c] Lap:%d   AI1[%c]:%d   AI2[%c]:%d   AI3[%c]:%d\n",
           cars[0].symbol, cars[0].laps,
           cars[1].symbol, cars[1].laps,
           cars[2].symbol, cars[2].laps,
           cars[3].symbol, cars[3].laps);
    printf("Controls: Arrow keys to drive, Q to quit.\n");

    /* Track area */
    for (int ty = 0; ty < TRACK_H; ++ty) {
        char line[SCREEN_W + 1];
        for (int x = 0; x < SCREEN_W; ++x)
            line[x] = ' ';
        line[SCREEN_W] = '\0';

        /* Copy track tiles into line buffer */
        for (int tx = 0; tx < TRACK_W; ++tx) {
            int sx = TRACK_OFFSET_X + tx;
            if (sx < 0 || sx >= SCREEN_W) continue;
            char t = track[ty][tx];
            if (t == 0)
                t = ' ';
            line[sx] = t;
        }

        /* Overlay cars */
        for (int i = 0; i < numCars; ++i) {
            int cx = (int)(cars[i].x + 0.5f);
            int cy = (int)(cars[i].y + 0.5f);
            if (cy != ty) continue;
            int sx = TRACK_OFFSET_X + cx;
            if (sx < 0 || sx >= SCREEN_W) continue;
            line[sx] = cars[i].symbol;
        }

        printf("%s\n", line);
    }
}

/* ---------- Main ---------- */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* unbuffered output for smoother updates */

#ifndef _WIN32
    enable_raw_mode();
#endif
    hide_cursor();

    init_track();

    Car cars[NUM_CARS];
    init_cars(cars);

    int running = 1;
    int winner = -1;

    while (running) {
        InputState playerInput;
        poll_player_input(&playerInput);
        if (playerInput.quit) {
            running = 0;
            break;
        }

        /* Update player car */
        update_car_physics(&cars[0], &playerInput, DT);

        /* Update AI cars */
        for (int i = 1; i < NUM_CARS; ++i) {
            InputState aiInput;
            compute_ai_input(&cars[i], &aiInput);
            update_car_physics(&cars[i], &aiInput, DT);
        }

        /* Car-car collisions */
        handle_car_collisions(cars, NUM_CARS);

        /* Lap / checkpoint */
        for (int i = 0; i < NUM_CARS; ++i) {
            update_lap_and_checkpoint(&cars[i]);
            if (cars[i].laps >= MAX_LAPS && winner == -1) {
                winner = i;
                running = 0;
            }
        }

        /* Render */
        render(cars, NUM_CARS, MAX_LAPS);

        sleep_ms((int)(DT * 1000.0f));
    }

    show_cursor();
#ifndef _WIN32
    disable_raw_mode();
#endif

    if (winner >= 0) {
        printf("\nRace finished! Winner: %s %c\n",
               (cars[winner].isPlayer ? "PLAYER" : "CPU"),
               cars[winner].symbol);
    } else {
        printf("\nRace aborted.\n");
    }

    printf("Press Enter to exit.\n");
    /* Consume a line (works both with and without raw mode; raw is off now) */
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* skip */ }

    return 0;
}
