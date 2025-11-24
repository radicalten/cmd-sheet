/* ASCII Super Sprint-like top-down racer
 * Single-file C, no external dependencies beyond standard C/POSIX.
 *
 * Tested/targeted for Linux/macOS terminals (ANSI escape + termios).
 *
 * Controls:
 *   W = accelerate
 *   S = brake / reverse
 *   A = turn left
 *   D = turn right
 *   Q = quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/select.h>
#include <math.h>
#include <limits.h>

#define WIDTH       60
#define HEIGHT      20
#define MAX_TRACK   (WIDTH * HEIGHT)
#define MAX_ENEMIES 3
#define MAX_LAPS    3

#define TICK_USEC   50000        /* 20 FPS (50 ms per frame) */
#define ACCEL       18.0f        /* acceleration (cells/s^2) */
#define FRICTION    0.90f        /* velocity damping per frame */
#define TURN_SPEED  3.5f         /* radians per second */
#define MAX_SPEED   20.0f        /* clamp speed (cells/s) */

#define PI          3.14159265f

#define LAP_STATE_BEFORE 0
#define LAP_STATE_AFTER  1

typedef struct {
    int x, y;
} Node;

typedef struct {
    float x, y;      /* position in grid coords (float) */
    float vx, vy;    /* velocity components */
    float angle;     /* heading in radians (0 = right, pi/2 = down) */
    int   laps;
    int   lap_state;
    int   track_index; /* nearest track index for lap logic */
} Player;

typedef struct {
    float pos;       /* position along track (0..track_len) */
    float speed;     /* cells per second */
    int   laps;
    int   lap_state;
} Enemy;

/* Global track data */
static Node track[MAX_TRACK];
static int  track_len = 0;

/* Terminal raw mode */
static struct termios orig_termios;
static int raw_enabled = 0;

static void disable_raw_mode(void) {
    if (raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_enabled = 0;
    }
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    raw_enabled = 1;
}

static int read_key_nonblock(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (r > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1)
            return (int)c;
    }
    return -1;
}

static double current_time_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static void clear_screen(void) {
    /* ANSI clear screen */
    printf("\x1b[2J");
}

static void move_cursor_home(void) {
    /* ANSI cursor to home */
    printf("\x1b[H");
}

/* Build a simple rectangular loop track inside the screen */
static void build_track(void) {
    int start_x = 5;
    int end_x   = WIDTH - 6;
    int top_y   = 3;
    int bottom_y= HEIGHT - 4;

    track_len = 0;

    /* top edge left->right */
    for (int x = start_x; x <= end_x; ++x)
        track[track_len++] = (Node){x, top_y};

    /* right edge top->bottom */
    for (int y = top_y + 1; y <= bottom_y; ++y)
        track[track_len++] = (Node){end_x, y};

    /* bottom edge right->left */
    for (int x = end_x - 1; x >= start_x; --x)
        track[track_len++] = (Node){x, bottom_y};

    /* left edge bottom->top (excluding corners already used) */
    for (int y = bottom_y - 1; y > top_y; --y)
        track[track_len++] = (Node){start_x, y};
}

static void set_road(char map[HEIGHT][WIDTH], int x, int y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (map[y][x] != 'S') {
            map[y][x] = '.';
        }
    }
}

/* Initialize map with walls and widen track around the central path */
static void init_map(char map[HEIGHT][WIDTH]) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            map[y][x] = '#';
        }
    }

    for (int i = 0; i < track_len; ++i) {
        int x = track[i].x;
        int y = track[i].y;
        set_road(map, x, y);
        set_road(map, x + 1, y);
        set_road(map, x - 1, y);
        set_road(map, x, y + 1);
        set_road(map, x, y - 1);
    }

    /* Start/finish line at track[0] */
    Node s = track[0];
    map[s.y][s.x] = 'S';
}

/* Find the closest track index to current float position */
static int nearest_track_index(float fx, float fy) {
    int col = (int)(fx + 0.5f);
    int row = (int)(fy + 0.5f);

    int best = 0;
    int bestd = INT_MAX;
    for (int i = 0; i < track_len; ++i) {
        int dx = track[i].x - col;
        int dy = track[i].y - row;
        int d = dx * dx + dy * dy;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

static void update_player(Player *p, char map[HEIGHT][WIDTH],
                          float dt, int accel, int brake,
                          int turn_left, int turn_right) {
    /* Turning */
    if (turn_left)
        p->angle -= TURN_SPEED * dt;
    if (turn_right)
        p->angle += TURN_SPEED * dt;

    float ax = 0.0f, ay = 0.0f;

    if (accel) {
        ax += cosf(p->angle) * ACCEL;
        ay += sinf(p->angle) * ACCEL;
    }
    if (brake) {
        ax -= cosf(p->angle) * ACCEL * 0.5f;
        ay -= sinf(p->angle) * ACCEL * 0.5f;
    }

    p->vx += ax * dt;
    p->vy += ay * dt;

    /* Speed clamp */
    float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
    if (speed > MAX_SPEED) {
        float scale = MAX_SPEED / speed;
        p->vx *= scale;
        p->vy *= scale;
    }

    /* Friction */
    p->vx *= FRICTION;
    p->vy *= FRICTION;

    /* Proposed new position */
    float newx = p->x + p->vx * dt;
    float newy = p->y + p->vy * dt;

    int col = (int)(newx + 0.5f);
    int row = (int)(newy + 0.5f);

    int blocked = 0;
    if (col < 0 || col >= WIDTH || row < 0 || row >= HEIGHT) {
        blocked = 1;
    } else {
        char tile = map[row][col];
        if (!(tile == '.' || tile == 'S'))
            blocked = 1;
    }

    if (blocked) {
        /* Simple collision: bounce and do not move */
        p->vx *= -0.3f;
        p->vy *= -0.3f;
    } else {
        p->x = newx;
        p->y = newy;
    }

    /* Update nearest track index (for lap detection) */
    p->track_index = nearest_track_index(p->x, p->y);
}

static void update_player_lap(Player *p) {
    int half = track_len / 2;
    int idx  = p->track_index;

    if (p->lap_state == LAP_STATE_BEFORE && idx > half) {
        p->lap_state = LAP_STATE_AFTER;
    } else if (p->lap_state == LAP_STATE_AFTER && idx < half) {
        p->laps++;
        p->lap_state = LAP_STATE_BEFORE;
    }
}

static void update_enemies(Enemy enemies[], int num, float dt) {
    int half = track_len / 2;
    for (int i = 0; i < num; ++i) {
        enemies[i].pos += enemies[i].speed * dt;
        /* Wrap around the track */
        while (enemies[i].pos >= (float)track_len)
            enemies[i].pos -= (float)track_len;
        while (enemies[i].pos < 0.0f)
            enemies[i].pos += (float)track_len;

        int idx = ((int)enemies[i].pos) % track_len;
        if (enemies[i].lap_state == LAP_STATE_BEFORE && idx > half) {
            enemies[i].lap_state = LAP_STATE_AFTER;
        } else if (enemies[i].lap_state == LAP_STATE_AFTER && idx < half) {
            enemies[i].laps++;
            enemies[i].lap_state = LAP_STATE_BEFORE;
        }
    }
}

static void render(const char map[HEIGHT][WIDTH],
                   const Player *p,
                   const Enemy enemies[], int num_enemies,
                   double start_time) {
    clear_screen();
    move_cursor_home();

    double t = current_time_sec() - start_time;
    if (t < 0) t = 0;
    int total_sec = (int)t;
    int min = total_sec / 60;
    int sec = total_sec % 60;
    int ms  = (int)((t - (double)total_sec) * 1000.0);

    printf("ASCII Super Sprint   Laps: %d/%d   Time: %02d:%02d.%03d\n",
           p->laps, MAX_LAPS, min, sec, ms);
    printf("Controls: W=accel  S=brake  A/D=turn  Q=quit\n");

    /* Player integer position and heading */
    int p_col = (int)(p->x + 0.5f);
    int p_row = (int)(p->y + 0.5f);

    float a = fmodf(p->angle, 2.0f * PI);
    if (a < 0.0f) a += 2.0f * PI;
    char pch;
    if (a >= 7.0f * PI / 4.0f || a < PI / 4.0f)
        pch = '>';
    else if (a < 3.0f * PI / 4.0f)
        pch = 'v';
    else if (a < 5.0f * PI / 4.0f)
        pch = '<';
    else
        pch = '^';

    int ex[MAX_ENEMIES], ey[MAX_ENEMIES];
    for (int i = 0; i < num_enemies; ++i) {
        int idx = ((int)enemies[i].pos) % track_len;
        ex[i] = track[idx].x;
        ey[i] = track[idx].y;
    }

    /* Draw map with overlayed cars */
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            char c = map[y][x];
            if (y == p_row && x == p_col) {
                c = pch;
            } else {
                for (int i = 0; i < num_enemies; ++i) {
                    if (y == ey[i] && x == ex[i]) {
                        c = (char)('1' + i);
                        break;
                    }
                }
            }
            putchar(c);
        }
        putchar('\n');
    }

    printf("AI laps: ");
    for (int i = 0; i < num_enemies; ++i) {
        printf("%d/%d ", enemies[i].laps, MAX_LAPS);
    }
    printf("\n");

    fflush(stdout);
}

static void show_result(int winner, double total_time,
                        const Player *p,
                        const Enemy enemies[], int num_enemies) {
    clear_screen();
    move_cursor_home();

    printf("Race finished!\n\n");
    if (winner == 1) {
        printf("You win!\n\n");
    } else if (winner == 2) {
        printf("You lost. An AI car won the race.\n\n");
    } else {
        printf("Race aborted.\n\n");
    }

    if (total_time < 0) total_time = 0;
    int total_sec = (int)total_time;
    int min = total_sec / 60;
    int sec = total_sec % 60;
    int ms  = (int)((total_time - (double)total_sec) * 1000.0);

    printf("Total time: %02d:%02d.%03d\n", min, sec, ms);
    printf("Your laps: %d / %d\n", p->laps, MAX_LAPS);

    for (int i = 0; i < num_enemies; ++i) {
        printf("AI #%d laps: %d / %d\n",
               i + 1, enemies[i].laps, MAX_LAPS);
    }

    printf("\nPress ENTER to exit...\n");
    fflush(stdout);

    /* Now in canonical mode, wait for ENTER */
    int ch;
    do {
        ch = getchar();
        if (ch == EOF) break;
    } while (ch != '\n');
}

int main(void) {
    enable_raw_mode();

    build_track();
    char map[HEIGHT][WIDTH];
    init_map(map);

    Player player;
    Node start = track[0];
    player.x = (float)start.x + 0.5f;
    player.y = (float)start.y + 0.5f;
    player.vx = player.vy = 0.0f;
    player.angle = 0.0f;
    player.laps = 0;
    player.lap_state = LAP_STATE_BEFORE;
    player.track_index = 0;

    Enemy enemies[MAX_ENEMIES];
    int num_enemies = 3;
    if (num_enemies > MAX_ENEMIES) num_enemies = MAX_ENEMIES;

    for (int i = 0; i < num_enemies; ++i) {
        enemies[i].pos = (float)track_len * (float)(i + 1) / (float)(num_enemies + 1);
        enemies[i].speed = 7.0f - 0.7f * (float)i; /* slightly different speeds */
        enemies[i].laps = 0;
        enemies[i].lap_state = LAP_STATE_BEFORE;
    }

    clear_screen();
    move_cursor_home();
    printf("ASCII Super Sprint\n\n");
    printf("Single-screen top-down racer.\n\n");
    printf("Controls:\n");
    printf("  W - Accelerate\n");
    printf("  S - Brake / reverse\n");
    printf("  A - Turn left\n");
    printf("  D - Turn right\n");
    printf("  Q - Quit\n\n");
    printf("First to %d laps wins.\n\n", MAX_LAPS);
    printf("Press any key to start...\n");
    fflush(stdout);

    while (read_key_nonblock() == -1) {
        usleep(10000);
    }

    double start_time = current_time_sec();
    double total_time = 0.0;
    int running = 1;
    int winner  = 0;
    const float dt = (float)TICK_USEC / 1000000.0f;

    while (running) {
        int accel = 0, brake = 0, left = 0, right = 0;

        int ch;
        /* Process all pending key presses this frame */
        while ((ch = read_key_nonblock()) != -1) {
            if (ch == 'q' || ch == 'Q') {
                running = 0;
                winner = 0;
                break;
            } else if (ch == 'w' || ch == 'W') {
                accel = 1;
            } else if (ch == 's' || ch == 'S') {
                brake = 1;
            } else if (ch == 'a' || ch == 'A') {
                left = 1;
            } else if (ch == 'd' || ch == 'D') {
                right = 1;
            }
        }
        if (!running)
            break;

        update_player(&player, map, dt, accel, brake, left, right);
        update_player_lap(&player);
        update_enemies(enemies, num_enemies, dt);

        if (player.laps >= MAX_LAPS) {
            winner = 1;
            running = 0;
        } else {
            for (int i = 0; i < num_enemies; ++i) {
                if (enemies[i].laps >= MAX_LAPS) {
                    if (winner == 0) {
                        winner = 2;
                    }
                    running = 0;
                    break;
                }
            }
        }

        render(map, &player, enemies, num_enemies, start_time);

        usleep(TICK_USEC);
    }

    total_time = current_time_sec() - start_time;

    disable_raw_mode();
    show_result(winner, total_time, &player, enemies, num_enemies);

    return 0;
}
