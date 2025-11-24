// Minimal single-file ASCII top-down racing game.
// Compile with:  gcc -std=c99 -O2 -Wall -Wextra racer.c -lm -o racer

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

#define MAP_W 40
#define MAP_H 17

#define FPS 30
#define DT (1.0f / FPS)

#define ACCELERATION 20.0f
#define BRAKE_ACCEL 25.0f
#define FRICTION 3.0f
#define MAX_SPEED 12.0f
#define TURN_RATE 3.5f

#define LAPS_TO_WIN 3

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Track layout: '#' = wall, '.' = road, 'S' = start/finish line
static const char TRACK[MAP_H][MAP_W + 1] = {
    "########################################",
    "#......................................#",
    "#......................................#",
    "#...################............####...#",
    "#...#..............#............#..#...#",
    "#...#..............#............#..#...#",
    "#...#..............##########...#..#...#",
    "#...#...........................#..#...#",
    "#...#...........................#..#...#",
    "#...#..............##########...#..#...#",
    "#...#..............#............#..#...#",
    "#...#..............#............#..#...#",
    "#...################............####...#",
    "#......................................#",
    "#......................................#",
    "#..................................S...#",
    "########################################"
};

// Terminal raw mode handling
static struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // Show cursor again
    printf("\x1b[?25h");
    fflush(stdout);
}

void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Hide cursor and clear screen
    printf("\x1b[?25l");
    printf("\x1b[2J");
    printf("\x1b[H");
    fflush(stdout);
}

int kbhit(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int read_key(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
}

double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

// Car state
typedef struct {
    float x, y;
    float prev_x, prev_y;
    float speed;
    float angle;
    int laps;
    int finished;
} Car;

// Simple AI waypoints forming a big rectangle around the outer track
#define NUM_WP 5
static const float AI_WP[NUM_WP][2] = {
    {32.5f, 15.5f}, // near start, inside
    { 3.5f, 15.5f}, // bottom-left
    { 3.5f,  1.5f}, // top-left
    {36.5f,  1.5f}, // top-right
    {36.5f, 15.5f}  // bottom-right
};

float normalize_angle(float a) {
    while (a >  (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

// Integrate car physics, handle collisions and lap counting
void move_car(Car *c, float accel_input, float turn_input,
              float dt, int start_x, int start_y) {
    c->prev_x = c->x;
    c->prev_y = c->y;

    // Throttle/brake input (-1..1)
    if (accel_input > 0.0f) {
        c->speed += ACCELERATION * dt * accel_input;
    } else if (accel_input < 0.0f) {
        c->speed += BRAKE_ACCEL * dt * accel_input; // accel_input is negative => braking
    }

    // Friction
    if (c->speed > 0.0f) {
        c->speed -= FRICTION * dt;
        if (c->speed < 0.0f) c->speed = 0.0f;
    }

    // Clamp speed
    if (c->speed > MAX_SPEED) c->speed = MAX_SPEED;

    // Steering (only when moving a bit)
    if (c->speed > 0.5f) {
        float turn_scale = 0.4f + 0.6f * (c->speed / MAX_SPEED);
        c->angle += turn_input * TURN_RATE * dt * turn_scale;
        c->angle = normalize_angle(c->angle);
    }

    // Integrate position
    float new_x = c->x + cosf(c->angle) * c->speed * dt;
    float new_y = c->y + sinf(c->angle) * c->speed * dt;

    int tile_x = (int)new_x;
    int tile_y = (int)new_y;

    int blocked = 0;
    if (tile_x < 0 || tile_x >= MAP_W || tile_y < 0 || tile_y >= MAP_H) {
        blocked = 1;
    } else {
        char ch = TRACK[tile_y][tile_x];
        if (ch == '#') blocked = 1; // walls only
    }

    if (blocked) {
        // Hit a wall: stop
        c->speed = 0.0f;
    } else {
        c->x = new_x;
        c->y = new_y;
    }

    // Lap detection: crossing start line from right to left on its row
    if (!c->finished) {
        int prev_row = (int)c->prev_y;
        int curr_row = (int)c->y;
        if (prev_row == start_y && curr_row == start_y) {
            float line_x = start_x + 0.5f;
            if (c->prev_x > line_x && c->x <= line_x) {
                c->laps++;
                if (c->laps >= LAPS_TO_WIN)
                    c->finished = 1;
            }
        }
    }
}

void update_ai(Car *car, float dt, int start_x, int start_y, int *ai_wp_index) {
    int idx = *ai_wp_index;
    float tx = AI_WP[idx][0];
    float ty = AI_WP[idx][1];

    float dx = tx - car->x;
    float dy = ty - car->y;
    float dist = sqrtf(dx*dx + dy*dy);

    // Switch to next waypoint when close
    if (dist < 1.5f) {
        idx = (idx + 1) % NUM_WP;
        *ai_wp_index = idx;
        tx = AI_WP[idx][0];
        ty = AI_WP[idx][1];
        dx = tx - car->x;
        dy = ty - car->y;
        dist = sqrtf(dx*dx + dy*dy);
    }

    float target_angle = atan2f(dy, dx);
    float angle_diff = normalize_angle(target_angle - car->angle);

    float turn_input = 0.0f;
    if (angle_diff > 0.1f)      turn_input = 1.0f;
    else if (angle_diff < -0.1f) turn_input = -1.0f;

    float desired_speed = MAX_SPEED * 0.7f;
    if (fabsf(angle_diff) > 0.6f)
        desired_speed *= 0.4f; // slow down for sharp turns

    float accel_input = 0.0f;
    if (car->speed < desired_speed - 0.5f)
        accel_input = 1.0f;     // accelerate
    else if (car->speed > desired_speed + 0.5f)
        accel_input = -1.0f;    // brake a bit

    move_car(car, accel_input, turn_input, dt, start_x, start_y);
}

// Draw the HUD and track with player (1) and AI (2)
void draw(const Car *player, const Car *ai) {
    printf("\x1b[H"); // move cursor to top-left
    printf("ASCII Off-Road (WASD to drive, Q to quit)\n");
    printf("Player: lap %d/%d  speed %.1f   AI: lap %d/%d  speed %.1f\n",
           player->laps, LAPS_TO_WIN, player->speed,
           ai->laps, LAPS_TO_WIN, ai->speed);
    printf("\n");

    char buf[MAP_H][MAP_W + 1];
    for (int y = 0; y < MAP_H; ++y) {
        memcpy(buf[y], TRACK[y], MAP_W + 1);
    }

    int px = (int)player->x;
    int py = (int)player->y;
    if (px >= 0 && px < MAP_W && py >= 0 && py < MAP_H) {
        buf[py][px] = '1';
    }

    int ax = (int)ai->x;
    int ay = (int)ai->y;
    if (ax >= 0 && ax < MAP_W && ay >= 0 && ay < MAP_H) {
        if (buf[ay][ax] == '1')
            buf[ay][ax] = 'X'; // overlap
        else
            buf[ay][ax] = '2';
    }

    for (int y = 0; y < MAP_H; ++y) {
        fwrite(buf[y], 1, MAP_W, stdout);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

int main(void) {
    // Find start/finish tile ('S')
    int start_x = 0, start_y = 0;
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            if (TRACK[y][x] == 'S') {
                start_x = x;
                start_y = y;
            }
        }
    }

    Car player = {0};
    Car ai = {0};

    // Player starts just to the right of 'S', facing left
    player.x = start_x + 1.5f;
    player.y = start_y + 0.5f;
    player.prev_x = player.x;
    player.prev_y = player.y;
    player.speed = 0.0f;
    player.angle = (float)M_PI;
    player.laps = 0;
    player.finished = 0;

    // AI starts at its first waypoint
    ai.x = AI_WP[0][0];
    ai.y = AI_WP[0][1];
    ai.prev_x = ai.x;
    ai.prev_y = ai.y;
    ai.speed = 0.0f;
    ai.angle = (float)M_PI;
    ai.laps = 0;
    ai.finished = 0;

    int ai_wp_index = 0;

    enable_raw_mode();

    double last_time = now_sec();
    int running = 1;
    int winner = 0; // 0 none, 1 player, 2 AI, 3 tie

    while (running) {
        double now = now_sec();
        float dt = (float)(now - last_time);
        if (dt <= 0.0f) dt = DT;
        if (dt > 0.1f) dt = 0.1f; // clamp if paused
        last_time = now;

        // Input for this frame
        float accel_input = 0.0f;
        float turn_input = 0.0f;

        while (kbhit()) {
            int ch = read_key();
            if (ch == -1) break;
            if (ch == 'q' || ch == 'Q') {
                running = 0;
            } else if (ch == 'w' || ch == 'W') {
                accel_input = 1.0f;
            } else if (ch == 's' || ch == 'S') {
                accel_input = -1.0f;
            } else if (ch == 'a' || ch == 'A') {
                turn_input = -1.0f;
            } else if (ch == 'd' || ch == 'D') {
                turn_input = 1.0f;
            }
        }

        if (!player.finished)
            move_car(&player, accel_input, turn_input, dt, start_x, start_y);
        if (!ai.finished)
            update_ai(&ai, dt, start_x, start_y, &ai_wp_index);

        draw(&player, &ai);

        if (!winner) {
            if (player.finished && ai.finished)
                winner = 3;
            else if (player.finished)
                winner = 1;
            else if (ai.finished)
                winner = 2;

            if (winner) {
                running = 0;
            }
        }

        // Frame limiting
        double frame_time = now_sec() - now;
        double target = 1.0 / FPS;
        if (frame_time < target) {
            int us = (int)((target - frame_time) * 1e6);
            if (us > 0) usleep(us);
        }
    }

    disable_raw_mode();
    printf("\n");
    if (winner == 1)
        printf("You win! (%d laps)\n", LAPS_TO_WIN);
    else if (winner == 2)
        printf("AI wins. Try again! (%d laps)\n", LAPS_TO_WIN);
    else if (winner == 3)
        printf("It's a tie! (%d laps)\n", LAPS_TO_WIN);
    else
        printf("Race ended.\n");

    return 0;
}
