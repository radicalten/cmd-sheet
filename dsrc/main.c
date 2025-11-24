/*
 * Simple terminal figure-8 racing game
 *
 * - Single screen, top-down, ASCII-art, sideways figure-8 (∞) track
 * - Single-player time trial: 3 laps
 * - Lap counter, timer, best lap, victory screen
 * - Controls: WASD or arrow keys; q / ESC to quit
 *
 * Compile (POSIX terminal, e.g. Linux/macOS):
 *   cc -std=c99 -Wall -O2 race.c -o race -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH       50
#define HEIGHT      20
#define FRAME_TIME  (1.0/60.0)
#define TOTAL_LAPS  3
#define HUGE_TIME   1e9
#define PI          3.14159265358979323846

/* Track grid: 1 = road, 0 = off-track */
static int track[HEIGHT][WIDTH];

/* Track geometry info */
static int centerY;
static int leftCenterX, rightCenterX;
static int outerR, innerR;
static double trackLength;

/* Car state */
static double carX, carY;
static double carAngle;   /* radians, 0 = right, pi/2 = up */
static double carSpeed;

/* Race state */
static int    raceStarted = 0;
static int    raceFinished = 0;
static int    lapsCompleted = 0;
static double raceStartTime = 0.0;
static double totalRaceTime = 0.0;
static double bestLapTime = HUGE_TIME;
static double lapStartTime = 0.0;
static double distThisLap = 0.0;

/* Input state (per frame) */
static int keyAccel = 0;
static int keyBrake = 0;
static int keyLeft  = 0;
static int keyRight = 0;
static int keyQuit  = 0;

/* Terminal state */
static struct termios origTermios;
static int rawModeEnabled = 0;

/* Time in seconds (double) */
static double getTimeSeconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* Restore terminal */
static void disableRawMode(void) {
    if (rawModeEnabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
        rawModeEnabled = 0;
    }
    /* Show cursor */
    printf("\x1b[?25h");
    fflush(stdout);
}

/* Put terminal in raw mode */
static void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &origTermios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disableRawMode);

    struct termios raw = origTermios;
    raw.c_lflag &= ~(ECHO | ICANON);      /* no echo, noncanonical */
    raw.c_iflag &= ~(IXON | ICRNL);       /* no Ctrl-S/Q, no CR->NL */
    raw.c_oflag &= ~(OPOST);              /* no post-processing */
    raw.c_cc[VMIN]  = 0;                  /* non-blocking read */
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    rawModeEnabled = 1;

    /* Clear screen, home cursor, hide cursor */
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);
}

/* Format time as MM:SS.mmm */
static void formatTime(double t, char *buf, size_t len) {
    if (t < 0.0) t = 0.0;
    int minutes = (int)(t / 60.0);
    double rem  = t - minutes * 60.0;
    int seconds = (int)rem;
    int millis  = (int)((rem - seconds) * 1000.0 + 0.5);

    if (millis >= 1000) {
        millis -= 1000;
        seconds++;
    }
    if (seconds >= 60) {
        seconds -= 60;
        minutes++;
    }
    snprintf(buf, len, "%02d:%02d.%03d", minutes, seconds, millis);
}

/* Initialize figure-8 track and car position */
static void initTrack(void) {
    memset(track, 0, sizeof(track));

    centerY     = HEIGHT / 2;
    leftCenterX = WIDTH / 3;
    rightCenterX = (WIDTH * 2) / 3;

    outerR = HEIGHT / 3;
    if (outerR < 4) outerR = 4;
    innerR = outerR - 3;
    if (innerR < 1) innerR = 1;

    int outer2 = outerR * outerR;
    int inner2 = innerR * innerR;

    for (int y = 0; y < HEIGHT; y++) {
        int dy = y - centerY;
        for (int x = 0; x < WIDTH; x++) {
            int dx1 = x - leftCenterX;
            int dx2 = x - rightCenterX;
            int d1 = dx1 * dx1 + dy * dy;
            int d2 = dx2 * dx2 + dy * dy;

            int ring1 = (d1 >= inner2 && d1 <= outer2);
            int ring2 = (d2 >= inner2 && d2 <= outer2);
            if (ring1 || ring2) {
                track[y][x] = 1;
            }
        }
    }

    /* Place car near lower part of left loop */
    int startX = -1, startY = -1;
    for (int y = centerY; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (track[y][x]) {
                startX = x;
                startY = y;
                break;
            }
        }
        if (startX != -1) break;
    }

    if (startX == -1) {
        /* Fallback: any road cell */
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                if (track[y][x]) {
                    startX = x;
                    startY = y;
                    break;
                }
            }
            if (startX != -1) break;
        }
    }

    if (startX == -1) {
        fprintf(stderr, "Track generation failed.\n");
        exit(1);
    }

    carX = startX + 0.5;
    carY = startY + 0.5;
    carAngle = 0.0;    /* facing right */
    carSpeed = 0.0;

    /* Approximate lap length: 2 loops of mean radius */
    double meanRadius = 0.5 * (outerR + innerR);
    trackLength = 4.0 * PI * meanRadius;
}

/* Read keyboard input, set key flags for this frame */
static void processInput(void) {
    keyAccel = keyBrake = keyLeft = keyRight = keyQuit = 0;

    char buf[32];
    int n;
    while ((n = (int)read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];

            if (c == 27) {  /* ESC or escape sequence */
                if (i + 2 < n && buf[i + 1] == '[') {
                    char d = buf[i + 2];
                    if (d == 'A') keyAccel = 1;   /* Up */
                    else if (d == 'B') keyBrake = 1; /* Down */
                    else if (d == 'C') keyRight = 1; /* Right */
                    else if (d == 'D') keyLeft  = 1; /* Left */
                    i += 2;
                } else {
                    keyQuit = 1;
                }
            } else {
                if (c == 'q' || c == 'Q') keyQuit  = 1;
                else if (c == 'w' || c == 'W') keyAccel = 1;
                else if (c == 's' || c == 'S') keyBrake = 1;
                else if (c == 'a' || c == 'A') keyLeft  = 1;
                else if (c == 'd' || c == 'D') keyRight = 1;
            }
        }
    }
}

/* Update car physics; return distance moved this frame along track */
static double updateCar(double dt) {
    const double ACCEL     = 25.0;
    const double BRAKE     = 30.0;
    const double MAX_SPEED = 20.0;
    const double FRICTION  = 5.0;
    const double TURN_RATE = 2.8;  /* radians/sec at full speed */

    /* Acceleration / braking */
    if (keyAccel) {
        carSpeed += ACCEL * dt;
    }
    if (keyBrake) {
        carSpeed -= BRAKE * dt;
    }

    /* Natural friction when not using throttle */
    if (!keyAccel && !keyBrake) {
        if (carSpeed > 0.0) {
            carSpeed -= FRICTION * dt;
            if (carSpeed < 0.0) carSpeed = 0.0;
        } else if (carSpeed < 0.0) {
            carSpeed += FRICTION * dt;
            if (carSpeed > 0.0) carSpeed = 0.0;
        }
    }

    /* Clamp speed */
    if (carSpeed >  MAX_SPEED)        carSpeed =  MAX_SPEED;
    if (carSpeed < -MAX_SPEED * 0.5)  carSpeed = -MAX_SPEED * 0.5; /* limited reverse */

    /* Steering */
    double speedFactor = fabs(carSpeed) / MAX_SPEED;
    if (speedFactor > 1.0) speedFactor = 1.0;
    if (speedFactor < 0.1) speedFactor = 0.1; /* allow some steering when slow */

    if (keyLeft)  carAngle -= TURN_RATE * speedFactor * dt;
    if (keyRight) carAngle += TURN_RATE * speedFactor * dt;

    /* Normalize angle */
    if (carAngle > PI)      carAngle -= 2.0 * PI;
    else if (carAngle < -PI) carAngle += 2.0 * PI;

    double prevX = carX;
    double prevY = carY;

    double nx = carX + cos(carAngle) * carSpeed * dt;
    double ny = carY - sin(carAngle) * carSpeed * dt; /* screen y goes down */

    double distMoved = 0.0;

    int gx = (int)floor(nx + 0.5);
    int gy = (int)floor(ny + 0.5);

    if (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT && track[gy][gx]) {
        /* Still on road */
        carX = nx;
        carY = ny;
        double dx = carX - prevX;
        double dy = carY - prevY;
        distMoved = sqrt(dx * dx + dy * dy);
    } else {
        /* Off-track penalty: big slowdown, stay in place */
        carSpeed *= 0.2;
    }

    return distMoved;
}

/* Render HUD and track */
static void render(double now) {
    char timeBuf[32];
    char bestBuf[32];

    if (raceStarted) {
        formatTime(now - raceStartTime, timeBuf, sizeof(timeBuf));
    } else {
        snprintf(timeBuf, sizeof(timeBuf), "00:00.000");
    }

    if (bestLapTime < HUGE_TIME * 0.5) {
        formatTime(bestLapTime, bestBuf, sizeof(bestBuf));
    } else {
        snprintf(bestBuf, sizeof(bestBuf), "--:--.---");
    }

    int displayLap;
    if (raceFinished) {
        displayLap = TOTAL_LAPS;
    } else if (raceStarted) {
        displayLap = lapsCompleted + 1;
        if (displayLap > TOTAL_LAPS) displayLap = TOTAL_LAPS;
    } else {
        displayLap = 0;
    }

    /* Move cursor to top-left */
    printf("\x1b[H");
    printf("Lap: %d/%d   Time: %s   Best lap: %s\n",
           displayLap, TOTAL_LAPS, timeBuf, bestBuf);
    printf("Controls: arrows or WASD to drive, 'q' to quit. "
           "Track: sideways figure-8\n");

    int cx = (int)floor(carX + 0.5);
    int cy = (int)floor(carY + 0.5);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            char ch = ' ';
            if (track[y][x]) {
                int edge = 0;
                /* Check 4-neighbor edge to draw borders */
                int nx, ny;
                ny = y - 1; nx = x;
                if (ny < 0 || !track[ny][nx]) edge = 1;
                ny = y + 1; nx = x;
                if (!edge && (ny >= HEIGHT || !track[ny][nx])) edge = 1;
                ny = y; nx = x - 1;
                if (!edge && (nx < 0 || !track[ny][nx])) edge = 1;
                ny = y; nx = x + 1;
                if (!edge && (nx >= WIDTH || !track[ny][nx])) edge = 1;

                ch = edge ? '#' : '.';
            }
            if (x == cx && y == cy) {
                ch = '@';  /* car */
            }
            putchar(ch);
        }
        putchar('\n');
    }
    fflush(stdout);
}

/* Victory screen after race completion */
static void showVictoryScreen(void) {
    char totalBuf[32];
    char bestBuf[32];

    formatTime(totalRaceTime, totalBuf, sizeof(totalBuf));
    if (bestLapTime < HUGE_TIME * 0.5) {
        formatTime(bestLapTime, bestBuf, sizeof(bestBuf));
    } else {
        snprintf(bestBuf, sizeof(bestBuf), "--:--.---");
    }

    printf("\x1b[2J\x1b[H");
    printf("===== RACE COMPLETE =====\n\n");
    printf("Laps:       %d\n", TOTAL_LAPS);
    printf("Total time: %s\n", totalBuf);
    printf("Best lap:   %s\n", bestBuf);
    printf("\nPress 'q' or ESC to quit.\n");
    fflush(stdout);

    while (1) {
        char buf[16];
        int n = (int)read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                unsigned char c = (unsigned char)buf[i];
                if (c == 'q' || c == 'Q' || c == 27 || c == '\n' || c == '\r') {
                    return;
                }
            }
        } else {
            usleep(10000);
        }
    }
}

int main(void) {
    enableRawMode();
    initTrack();

    double lastTime = getTimeSeconds();

    while (1) {
        double now = getTimeSeconds();
        double dt = now - lastTime;
        if (dt < 0.0) dt = 0.0;
        if (dt > 0.1) dt = 0.1;  /* clamp big jumps */

        processInput();
        if (keyQuit) {
            break;
        }

        double distMoved = updateCar(dt);

        /* Start race when player actually moves */
        if (!raceStarted) {
            if (fabs(carSpeed) > 0.5 && distMoved > 0.0) {
                raceStarted = 1;
                raceStartTime = now;
                lapStartTime  = now;
                distThisLap   = 0.0;
                lapsCompleted = 0;
            }
        } else if (!raceFinished) {
            distThisLap += distMoved;
            if (distThisLap >= trackLength) {
                lapsCompleted++;
                double lapTime = now - lapStartTime;
                lapStartTime = now;
                distThisLap -= trackLength;
                if (lapTime < bestLapTime) bestLapTime = lapTime;
                if (lapsCompleted >= TOTAL_LAPS) {
                    raceFinished = 1;
                    totalRaceTime = now - raceStartTime;
                }
            }
        }

        render(now);

        if (raceFinished) {
            break;
        }

        double frameEnd = getTimeSeconds();
        double frameElapsed = frameEnd - now;
        double sleepTime = FRAME_TIME - frameElapsed;
        if (sleepTime > 0.0) {
            usleep((useconds_t)(sleepTime * 1000000.0));
        }

        lastTime = now;
    }

    if (raceFinished) {
        showVictoryScreen();
    }

    return 0;
}
