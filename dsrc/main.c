// outrun.c
// Simple pseudo-3D ASCII road game inspired by OutRun.
// Single file, no external dependencies beyond POSIX terminal APIs.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#define SCREEN_W 80
#define SCREEN_H 24
#define HORIZON  (SCREEN_H / 3)
#define MAX_SEGMENTS 64

struct termios orig_termios;
int orig_fl = -1;

typedef struct {
    float curve;   // lateral curvature (screen-space units per world unit)
    float length;  // length of this segment in world units
} Segment;

Segment road[] = {
    { 0.0f,  40.0f},  // straight
    { 0.4f,  40.0f},  // gentle right
    { 0.0f,  30.0f},  // straight
    {-0.7f,  80.0f},  // strong left
    { 0.0f,  40.0f},  // straight
    { 0.5f,  60.0f},  // right
    { 0.0f,  40.0f},  // straight
    {-0.4f,  70.0f},  // left
    { 0.0f,  40.0f}   // straight
};
const int NUM_SEGMENTS = (int)(sizeof(road) / sizeof(road[0]));

float segStartZ[MAX_SEGMENTS];
float segEndZ  [MAX_SEGMENTS];
float segStartC[MAX_SEGMENTS];
float trackLength = 0.0f;

static float clampf(float x, float a, float b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

static double now_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    if (orig_fl != -1) {
        fcntl(STDIN_FILENO, F_SETFL, orig_fl);
    }
    // show cursor
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    orig_fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (orig_fl == -1) orig_fl = 0;
    fcntl(STDIN_FILENO, F_SETFL, orig_fl | O_NONBLOCK);

    // hide cursor, clear screen, move cursor home
    write(STDOUT_FILENO, "\x1b[2J\x1b[H\x1b[?25l", 13);
}

void handle_sigint(int sig) {
    (void)sig;
    exit(0); // atexit handler will restore terminal
}

void init_road(void) {
    if (NUM_SEGMENTS > MAX_SEGMENTS) {
        fprintf(stderr, "Too many segments; increase MAX_SEGMENTS.\n");
        exit(1);
    }

    trackLength = 0.0f;
    float center = 0.0f;
    for (int i = 0; i < NUM_SEGMENTS; ++i) {
        segStartZ[i] = trackLength;
        segStartC[i] = center;
        trackLength += road[i].length;
        segEndZ[i] = trackLength;
        center += road[i].curve * road[i].length;
    }
}

// z must be in [0, trackLength)
float get_road_center(float z) {
    int i;
    // Few segments, linear search is fine
    for (i = NUM_SEGMENTS - 1; i >= 0; --i) {
        if (z >= segStartZ[i]) break;
    }
    if (i < 0) i = 0;
    float dz = z - segStartZ[i];
    return segStartC[i] + road[i].curve * dz;
}

int main(void) {
    enable_raw_mode();
    signal(SIGINT, handle_sigint);
    init_road();

    float playerX = 0.0f;   // lateral position
    float playerZ = 0.0f;   // distance along track
    float speed   = 60.0f;  // world units / second
    float distanceTravelled = 0.0f;

    const float maxDistAhead = 300.0f;
    const float minSpeed     = 20.0f;
    const float maxSpeed     = 220.0f;
    const float cruiseSpeed  = 80.0f;

    double prevTime = now_seconds();
    int running = 1;

    while (running) {
        double now = now_seconds();
        float dt = (float)(now - prevTime);
        if (dt <= 0.0f) dt = 0.016f;
        if (dt > 0.1f)  dt = 0.1f;
        prevTime = now;

        // INPUT
        float steerInput = 0.0f;
        float accelInput = 0.0f;
        for (;;) {
            unsigned char c;
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n <= 0) break;
            if (c == 'q' || c == 'Q') {
                running = 0;
                break;
            } else if (c == 'a' || c == 'A') {
                steerInput -= 1.0f;
            } else if (c == 'd' || c == 'D') {
                steerInput += 1.0f;
            } else if (c == 'w' || c == 'W') {
                accelInput += 1.0f;
            } else if (c == 's' || c == 'S') {
                accelInput -= 1.0f;
            }
        }

        // UPDATE PHYSICS

        // Adjust speed with player accel and auto cruise
        speed += accelInput * 80.0f * dt;
        speed += (cruiseSpeed - speed) * 0.5f * dt;
        speed = clampf(speed, minSpeed, maxSpeed);

        // Steering; stronger effect at higher speed
        playerX += steerInput * (1.5f + speed / 100.0f) * dt;
        // natural centering
        playerX *= (1.0f - 1.5f * dt);
        playerX = clampf(playerX, -3.0f, 3.0f);

        // Move forward
        playerZ += speed * dt;
        distanceTravelled += speed * dt;
        if (playerZ >= trackLength)
            playerZ -= trackLength;

        // RENDER
        static char screen[SCREEN_H][SCREEN_W + 1];

        // Clear buffer
        for (int y = 0; y < SCREEN_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x)
                screen[y][x] = ' ';
            screen[y][SCREEN_W] = '\0';
        }

        // Simple sky and horizon
        for (int y = 0; y <= HORIZON; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                char ch = ' ';
                if (y == HORIZON) {
                    ch = '-'; // horizon line
                } else if (y == 1 && x == SCREEN_W - 10) {
                    ch = 'O'; // tiny "sun"
                }
                screen[y][x] = ch;
            }
        }

        int roadLeftBottom = 0;
        int roadRightBottom = SCREEN_W - 1;

        for (int y = HORIZON + 1; y < SCREEN_H; ++y) {
            float perspective = (float)(SCREEN_H - y) /
                                (float)(SCREEN_H - HORIZON);
            if (perspective < 0.0f) perspective = 0.0f;

            float distAhead = perspective * maxDistAhead; // 0 near, max far
            float sampleZ = playerZ + distAhead;
            if (sampleZ >= trackLength)
                sampleZ -= trackLength;

            float worldCenter = get_road_center(sampleZ);
            float relCenter   = worldCenter - playerX;

            // Road width narrows into distance
            float roadWidth = (float)SCREEN_W * (0.15f + 0.35f * perspective);
            float screenCenterF = (float)SCREEN_W * 0.5f +
                                  relCenter * perspective * 1.5f;

            int screenCenter = (int)(screenCenterF + 0.5f);
            int left  = (int)(screenCenterF - roadWidth * 0.5f);
            int right = (int)(screenCenterF + roadWidth * 0.5f);

            if (left < 0) left = 0;
            if (right >= SCREEN_W) right = SCREEN_W - 1;

            if (y == SCREEN_H - 1) {
                roadLeftBottom = left;
                roadRightBottom = right;
            }

            for (int x = 0; x < SCREEN_W; ++x) {
                if (x < left || x > right) {
                    // "grass"
                    screen[y][x] = ((x / 2 + y) & 1) ? '\'' : '.';
                } else {
                    // road
                    if (x == left || x == right) {
                        screen[y][x] = '|'; // edge
                    } else {
                        screen[y][x] = '.'; // asphalt
                    }
                }
            }

            // Center dashed line
            int stripe = ((int)(sampleZ / 10.0f)) & 1;
            if (stripe == 0) {
                int cx = screenCenter;
                if (cx > left + 1 && cx < right - 1 &&
                    cx >= 0 && cx < SCREEN_W) {
                    screen[y][cx] = ':';
                }
            }
        }

        // Determine if car is on road based on bottom row
        int carX = SCREEN_W / 2;
        int carY = SCREEN_H - 3;
        int onRoad = (carX > roadLeftBottom + 1 && carX < roadRightBottom - 1);

        if (!onRoad) {
            // Slow down when off road
            speed -= 100.0f * dt;
            if (speed < minSpeed) speed = minSpeed;
        }

        // Draw car (simple ASCII)
        if (carY >= 0 && carY < SCREEN_H) {
            if (carX >= 0 && carX < SCREEN_W) {
                screen[carY][carX] = 'A';
                if (carY + 1 < SCREEN_H) {
                    if (carX - 1 >= 0) screen[carY + 1][carX - 1] = '/';
                    if (carX + 1 < SCREEN_W) screen[carY + 1][carX + 1] = '\\';
                }
            }
        }

        // HUD
        char hud1[SCREEN_W + 1];
        snprintf(hud1, SCREEN_W, "ASCII Outrun-ish   A/D steer  W/S speed  Q quit");
        memcpy(screen[0], hud1, strlen(hud1));

        char hud2[SCREEN_W + 1];
        snprintf(hud2, SCREEN_W, "Speed:%3.0f  Dist:%7.0f  %s",
                 speed, distanceTravelled, onRoad ? "" : "OFF ROAD!");
        size_t len2 = strlen(hud2);
        if (len2 > SCREEN_W) len2 = SCREEN_W;
        memcpy(screen[1], hud2, len2);

        // Output buffer
        write(STDOUT_FILENO, "\x1b[H", 3); // home cursor
        for (int y = 0; y < SCREEN_H; ++y) {
            write(STDOUT_FILENO, screen[y], SCREEN_W);
            write(STDOUT_FILENO, "\r\n", 2);
        }

        // Frame cap ~30 FPS
        const float targetFrame = 1.0f / 30.0f;
        if (dt < targetFrame) {
            useconds_t us = (useconds_t)((targetFrame - dt) * 1000000.0f);
            if (us > 0) usleep(us);
        }
    }

    return 0;
}
