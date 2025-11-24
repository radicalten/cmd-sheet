/* Minimal ASCII top-down figure-8 racing game.
 * Single file, no external dependencies beyond libc/POSIX.
 * Tested with gcc on a Unix-like terminal.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <math.h>
#include <signal.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCREEN_W     80
#define TRACK_H      20
#define SCREEN_H     24
#define MAX_CARS     4
#define PATH_POINTS  512
#define LAPS_TO_WIN  5

static char  track[TRACK_H][SCREEN_W];
static float pathX[PATH_POINTS], pathY[PATH_POINTS];

static struct termios orig_termios;
static int term_configured = 0;

enum {
    COLOR_DEFAULT = 0,
    COLOR_WALL,
    COLOR_ROAD,
    COLOR_CAR1,
    COLOR_CAR2,
    COLOR_CAR3,
    COLOR_CAR4
};

static const char *colors[] = {
    "\x1b[0m",       /* default */
    "\x1b[1;37m",    /* walls */
    "\x1b[0;37m",    /* road */
    "\x1b[1;31m",    /* car 1 (player) */
    "\x1b[1;34m",    /* car 2 */
    "\x1b[1;32m",    /* car 3 */
    "\x1b[1;35m"     /* car 4 */
};

typedef struct {
    float x, y;
    float angle;
    float speed;
    float aiBaseSpeed;
    int   colorIndex;
    char  symbol;
    int   isPlayer;
    int   pathIndex;
    int   totalProgress;
    int   laps;
} Car;

typedef struct {
    int accelerate;
    int brake;
    int left;
    int right;
    int quit;
    int pause;
} InputState;

/* --- Terminal handling --------------------------------------------------- */

static void disable_raw_mode(void)
{
    if (!term_configured) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    term_configured = 0;
    fputs("\x1b[0m\x1b[?25h\x1b[2J\x1b[H", stdout);
    fflush(stdout);
}

static void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    term_configured = 1;
    atexit(disable_raw_mode);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    fputs("\x1b[?25l\x1b[2J\x1b[H", stdout);
    fflush(stdout);
}

static void handle_sigint(int sig)
{
    (void)sig;
    exit(0);
}

static double now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

/* --- Track generation: sideways figure-8 -------------------------------- */

static void build_track(void)
{
    int x, y;
    for (y = 0; y < TRACK_H; ++y)
        for (x = 0; x < SCREEN_W; ++x)
            track[y][x] = ' ';

    static int road[TRACK_H][SCREEN_W];
    memset(road, 0, sizeof(road));

    float cx = SCREEN_W / 2.0f;
    float cy = TRACK_H / 2.0f;
    float A  = (SCREEN_W - 10) / 3.0f;  /* horizontal size of 8 */
    float B  = (TRACK_H - 4) / 2.0f;    /* vertical size of 8   */

    int i;
    for (i = 0; i < PATH_POINTS; ++i) {
        float t  = (float)(2.0 * M_PI * i / PATH_POINTS);
        float px = cx + A * sinf(2.0f * t); /* sideways figure-8 */
        float py = cy + B * sinf(t);
        pathX[i] = px;
        pathY[i] = py;

        int dx, dy;
        for (dy = -3; dy <= 3; ++dy) {
            for (dx = -3; dx <= 3; ++dx) {
                float fx = px + dx;
                float fy = py + dy;
                int gx = (int)(fx + 0.5f);
                int gy = (int)(fy + 0.5f);
                if (gx < 0 || gx >= SCREEN_W || gy < 0 || gy >= TRACK_H)
                    continue;
                float dist = sqrtf((float)(dx * dx + dy * dy));
                if (dist <= 2.5f)
                    road[gy][gx] = 1;
            }
        }
    }

    /* Convert road mask to track characters */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            if (road[y][x])
                track[y][x] = '.';
            else
                track[y][x] = ' ';
        }
    }

    /* Wall tiles bordering the road */
    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            if (road[y][x])
                continue;
            int ny, nx;
            int wall = 0;
            for (ny = y - 1; ny <= y + 1 && !wall; ++ny) {
                for (nx = x - 1; nx <= x + 1; ++nx) {
                    if (nx < 0 || nx >= SCREEN_W || ny < 0 || ny >= TRACK_H)
                        continue;
                    if (road[ny][nx]) {
                        wall = 1;
                        break;
                    }
                }
            }
            if (wall)
                track[y][x] = '#';
            else
                track[y][x] = ' ';
        }
    }

    /* Outer border walls */
    for (x = 0; x < SCREEN_W; ++x) {
        track[0][x]           = '#';
        track[TRACK_H - 1][x] = '#';
    }
    for (y = 0; y < TRACK_H; ++y) {
        track[y][0]              = '#';
        track[y][SCREEN_W - 1]   = '#';
    }

    /* Start/finish line marker at path index 0 */
    int sx = (int)(pathX[0] + 0.5f);
    int sy = (int)(pathY[0] + 0.5f);
    int yy;
    for (yy = sy - 1; yy <= sy + 1; ++yy) {
        if (yy >= 0 && yy < TRACK_H && track[yy][sx] != '#')
            track[yy][sx] = '|';
    }
}

/* --- Path progress / waypoints ------------------------------------------ */

static int find_nearest_wp(const Car *c)
{
    int   best  = 0;
    float bestD = 1e30f;
    int   i;
    for (i = 0; i < PATH_POINTS; ++i) {
        float dx = pathX[i] - c->x;
        float dy = pathY[i] - c->y;
        float d2 = dx * dx + dy * dy;
        if (d2 < bestD) {
            bestD = d2;
            best  = i;
        }
    }
    return best;
}

static void update_progress(Car *c)
{
    int nearest = find_nearest_wp(c);
    if (c->pathIndex < 0) {
        c->pathIndex     = nearest;
        c->totalProgress = nearest;
        c->laps          = c->totalProgress / PATH_POINTS;
        return;
    }
    int diff = nearest - c->pathIndex;
    if (diff > PATH_POINTS / 2)
        diff -= PATH_POINTS;
    else if (diff < -PATH_POINTS / 2)
        diff += PATH_POINTS;

    c->totalProgress += diff;
    if (c->totalProgress < 0)
        c->totalProgress = 0;
    c->pathIndex = nearest;
    c->laps      = c->totalProgress / PATH_POINTS;
}

/* --- AI control and car physics ----------------------------------------- */

static void compute_ai_control(Car *c, int *outAccel, int *outBrake, int *outSteer)
{
    int   lookahead = 20;
    int   target    = (c->pathIndex + lookahead) % PATH_POINTS;
    float tx        = pathX[target];
    float ty        = pathY[target];
    float dx        = tx - c->x;
    float dy        = ty - c->y;
    float targetAng = atan2f(dy, dx);
    float diff      = targetAng - c->angle;

    while (diff > (float)M_PI)  diff -= 2.0f * (float)M_PI;
    while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;

    int steer = 0;
    if (diff > 0.1f)      steer = +1;
    else if (diff < -0.1f) steer = -1;

    float absDiff      = diff > 0 ? diff : -diff;
    float desiredSpeed = c->aiBaseSpeed *
                         (1.0f - 0.4f * absDiff / (float)M_PI);
    if (desiredSpeed < 10.0f)
        desiredSpeed = 10.0f;

    int accel = 0, brake = 0;
    if (c->speed < desiredSpeed) accel = 1;
    else                          brake = 1;

    *outAccel = accel;
    *outBrake = brake;
    *outSteer = steer;
}

static void update_car(Car *c, float dt, int accelKey, int brakeKey, int steerDir)
{
    int  gx   = (int)(c->x + 0.5f);
    int  gy   = (int)(c->y + 0.5f);
    char tile = ' ';
    if (gx >= 0 && gx < SCREEN_W && gy >= 0 && gy < TRACK_H)
        tile = track[gy][gx];

    float friction;
    if (tile == '.' || tile == '|')
        friction = 2.0f;
    else if (tile == '#')
        friction = 6.0f;
    else
        friction = 3.0f;

    const float ACCEL_RATE = 80.0f;
    const float BRAKE_RATE = 60.0f;
    const float MAX_SPEED  = 40.0f;
    const float TURN_RATE  = 2.8f;

    float accel = 0.0f;
    if (accelKey) accel += ACCEL_RATE;
    if (brakeKey) accel -= BRAKE_RATE;

    c->speed += (accel - friction * c->speed) * dt;
    if (c->speed < 0.0f) c->speed = 0.0f;
    if (c->speed > MAX_SPEED) c->speed = MAX_SPEED;

    float speedFactor = 0.3f + 0.7f * (c->speed / MAX_SPEED);
    c->angle += steerDir * TURN_RATE * dt * speedFactor;

    float dx = cosf(c->angle) * c->speed * dt;
    float dy = sinf(c->angle) * c->speed * dt;
    float newX = c->x + dx;
    float newY = c->y + dy;

    int nx = (int)(newX + 0.5f);
    int ny = (int)(newY + 0.5f);
    int collided = 0;

    if (nx < 0 || nx >= SCREEN_W || ny < 0 || ny >= TRACK_H)
        collided = 1;
    else if (track[ny][nx] == '#')
        collided = 1;

    if (!collided) {
        c->x = newX;
        c->y = newY;
    } else {
        c->speed *= 0.3f;
    }
}

/* --- Input -------------------------------------------------------------- */

static void read_input(InputState *in)
{
    memset(in, 0, sizeof(*in));
    unsigned char buf[64];
    int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    int i;
    for (i = 0; i < n; ++i) {
        unsigned char c = buf[i];
        if (c == 'q' || c == 'Q') {
            in->quit = 1;
        } else if (c == 'p' || c == 'P') {
            in->pause = 1;
        } else if (c == 'w' || c == 'W') {
            in->accelerate = 1;
        } else if (c == 's' || c == 'S') {
            in->brake = 1;
        } else if (c == 'a' || c == 'A') {
            in->left = 1;
        } else if (c == 'd' || c == 'D') {
            in->right = 1;
        } else if (c == '\033' && i + 2 < n && buf[i + 1] == '[') {
            unsigned char code = buf[i + 2];
            if (code == 'A')      in->accelerate = 1; /* Up */
            else if (code == 'B') in->brake      = 1; /* Down */
            else if (code == 'C') in->right      = 1; /* Right */
            else if (code == 'D') in->left       = 1; /* Left  */
            i += 2;
        }
    }
}

/* --- Rendering ---------------------------------------------------------- */

static void draw_frame(Car *cars, int numCars, int gameState, int paused)
{
    char fb[TRACK_H][SCREEN_W];
    int x, y;

    for (y = 0; y < TRACK_H; ++y)
        for (x = 0; x < SCREEN_W; ++x)
            fb[y][x] = track[y][x];

    int i;
    for (i = 0; i < numCars; ++i) {
        Car *c = &cars[i];
        int ix = (int)(c->x + 0.5f);
        int iy = (int)(c->y + 0.5f);
        if (ix >= 0 && ix < SCREEN_W && iy >= 0 && iy < TRACK_H)
            fb[iy][ix] = c->symbol;
    }

    fputs("\x1b[H", stdout);
    int currentColor = -1;

    for (y = 0; y < TRACK_H; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            char ch = fb[y][x];
            int colorId;
            if (ch == '#')                      colorId = COLOR_WALL;
            else if (ch == '.' || ch == '|')    colorId = COLOR_ROAD;
            else if (ch == '1')                 colorId = COLOR_CAR1;
            else if (ch == '2')                 colorId = COLOR_CAR2;
            else if (ch == '3')                 colorId = COLOR_CAR3;
            else if (ch == '4')                 colorId = COLOR_CAR4;
            else                                colorId = COLOR_DEFAULT;

            if (colorId != currentColor) {
                fputs(colors[colorId], stdout);
                currentColor = colorId;
            }
            fputc(ch, stdout);
        }
        fputc('\n', stdout);
    }
    if (currentColor != COLOR_DEFAULT)
        fputs(colors[COLOR_DEFAULT], stdout);

    /* Ranking for HUD */
    int order[MAX_CARS];
    for (i = 0; i < numCars; ++i) order[i] = i;
    int j;
    for (i = 0; i < numCars - 1; ++i) {
        for (j = 0; j < numCars - 1 - i; ++j) {
            if (cars[order[j]].totalProgress <
                cars[order[j + 1]].totalProgress) {
                int tmp      = order[j];
                order[j]     = order[j + 1];
                order[j + 1] = tmp;
            }
        }
    }

    int playerPos = 1;
    for (i = 0; i < numCars; ++i)
        if (order[i] == 0)
            playerPos = i + 1;

    Car *p = &cars[0];
    int shownLap = p->laps + 1;
    if (shownLap > LAPS_TO_WIN) shownLap = LAPS_TO_WIN;

    printf("Lap %d / %d   Position: %d / %d   Speed: %3.0f\n",
           shownLap, LAPS_TO_WIN, playerPos, numCars, p->speed);

    if (gameState == 0) {
        if (paused)
            printf("PAUSED - Press P to resume, Q to quit\n");
        else
            printf("Controls: Arrow keys or WASD to drive  |  P = pause  |  Q = quit\n");
    } else {
        printf("Race finished! You placed %d / %d. Press Q to quit.\n",
               playerPos, numCars);
    }

    fflush(stdout);
}

/* --- Car initialization -------------------------------------------------- */

static void init_cars(Car *cars, int *outNum)
{
    int   num   = 4;
    int   i;
    float dx    = pathX[1] - pathX[0];
    float dy    = pathY[1] - pathY[0];
    float angle = atan2f(dy, dx);
    float px    = pathX[0];
    float py    = pathY[0];
    float perpX = -sinf(angle);
    float perpY =  cosf(angle);

    for (i = 0; i < num; ++i) {
        Car *c     = &cars[i];
        c->isPlayer = (i == 0);
        c->symbol   = '1' + i;
        c->colorIndex = COLOR_CAR1 + i;

        float base   = -1.8f;
        float step   =  1.2f;
        float offset = base + step * i;

        c->x = px + perpX * offset;
        c->y = py + perpY * offset;

        c->angle         = angle;
        c->speed         = 0.0f;
        c->pathIndex     = -1;
        c->totalProgress = 0;
        c->laps          = 0;
        c->aiBaseSpeed   = c->isPlayer ? 0.0f : (24.0f + 2.0f * i);
    }
    *outNum = num;
}

/* --- Main ---------------------------------------------------------------- */

int main(void)
{
    enable_raw_mode();
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);

    build_track();

    Car cars[MAX_CARS];
    int numCars;
    init_cars(cars, &numCars);

    /* Initialize progress before starting loop */
    int i;
    for (i = 0; i < numCars; ++i)
        update_progress(&cars[i]);

    fputs("ASCII Off-road style racing\n", stdout);
    fputs("Press any key to start...\n", stdout);
    fflush(stdout);

    /* Wait for a single key press */
    unsigned char ch;
    while (read(STDIN_FILENO, &ch, 1) <= 0) {
        usleep(10000);
    }

    const double frameTime = 1.0 / 30.0;
    int running   = 1;
    int paused    = 0;
    int gameState = 0;

    while (running) {
        double frameStart = now_sec();

        InputState input;
        read_input(&input);
        if (input.quit) break;
        if (input.pause) paused = !paused;

        if (!paused && gameState == 0) {
            float dt = (float)frameTime;

            /* Player */
            Car *p = &cars[0];
            int steer = 0;
            if (input.left)  steer -= 1;
            if (input.right) steer += 1;
            int accel = input.accelerate ? 1 : 0;
            int brake = input.brake      ? 1 : 0;

            update_car(p, dt, accel, brake, steer);
            update_progress(p);

            /* AI cars */
            for (i = 1; i < numCars; ++i) {
                Car *c = &cars[i];
                if (c->aiBaseSpeed <= 0.0f) continue;
                int aiAccel, aiBrake, aiSteer;
                compute_ai_control(c, &aiAccel, &aiBrake, &aiSteer);
                update_car(c, dt, aiAccel, aiBrake, aiSteer);
                update_progress(c);
            }

            if (p->laps >= LAPS_TO_WIN) {
                gameState = 1;
            }
        }

        draw_frame(cars, numCars, gameState, paused);

        double frameEnd = now_sec();
        double used     = frameEnd - frameStart;
        if (used < frameTime) {
            usleep((unsigned int)((frameTime - used) * 1000000.0));
        }
    }

    return 0;
}
