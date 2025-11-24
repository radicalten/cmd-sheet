/* 
 * Simple single-screen ASCII top-down racing game.
 *
 * - Single circuit track
 * - Player car (WASD controls)
 * - Enemy cars that auto-drive the circuit
 * - Lap counter
 * - Timer
 * - Victory/defeat screen
 * - Uses ANSI escape codes to update screen in-place
 *
 * Build (Linux / other POSIX):
 *   gcc -O2 -std=c99 -Wall -Wextra race.c -o race
 *
 * Run in an 80x24 terminal. Quit with 'q'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <string.h>

#define WIDTH       80
#define HEIGHT      24
#define MAX_PATH    1024
#define NUM_ENEMIES 3
#define LAPS_TO_WIN 3

/* ---------- Terminal handling ---------- */

static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    /* Show cursor */
    printf("\x1b[?25h");
    fflush(stdout);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    /* Hide cursor, clear screen, move home */
    printf("\x1b[?25l");
    printf("\x1b[2J");
    printf("\x1b[H");
    fflush(stdout);
}

/* ---------- Time helpers ---------- */

static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* ---------- Track and path ---------- */

static char track_template[HEIGHT][WIDTH];
static int  path_x[MAX_PATH];
static int  path_y[MAX_PATH];
static int  path_len = 0;

static void init_track(void) {
    // Fill with spaces
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            track_template[y][x] = ' ';

    // Outer frame of the track area
    for (int x = 0; x < WIDTH; x++) {
        if (2 >= 0 && 2 < HEIGHT)       track_template[2][x] = '#';
        if (HEIGHT - 3 >= 0)            track_template[HEIGHT - 3][x] = '#';
    }
    for (int y = 2; y <= HEIGHT - 3; y++) {
        track_template[y][0]        = '#';
        track_template[y][WIDTH-1]  = '#';
    }

    // Rectangular circuit path inside
    int x0 = 10;
    int x1 = WIDTH - 11;   // 69 for WIDTH=80
    int y0 = 4;
    int y1 = HEIGHT - 5;   // 19 for HEIGHT=24

    path_len = 0;

    // Top edge
    for (int x = x0; x <= x1 && path_len < MAX_PATH; x++) {
        path_x[path_len] = x;
        path_y[path_len] = y0;
        track_template[y0][x] = '.';
        path_len++;
    }
    // Right edge (below top)
    for (int y = y0 + 1; y <= y1 && path_len < MAX_PATH; y++) {
        path_x[path_len] = x1;
        path_y[path_len] = y;
        track_template[y][x1] = '.';
        path_len++;
    }
    // Bottom edge (leftwards)
    for (int x = x1 - 1; x >= x0 && path_len < MAX_PATH; x--) {
        path_x[path_len] = x;
        path_y[path_len] = y1;
        track_template[y1][x] = '.';
        path_len++;
    }
    // Left edge (upwards, excluding corners)
    for (int y = y1 - 1; y > y0 && path_len < MAX_PATH; y--) {
        path_x[path_len] = x0;
        path_y[path_len] = y;
        track_template[y][x0] = '.';
        path_len++;
    }

    // Mark start/finish line at path index 0
    if (path_len > 0) {
        int sx = path_x[0];
        int sy = path_y[0];
        track_template[sy][sx] = 'S';
    }
}

static int path_index_for(int x, int y) {
    for (int i = 0; i < path_len; i++) {
        if (path_x[i] == x && path_y[i] == y)
            return i;
    }
    return -1;
}

/* ---------- Game state ---------- */

typedef struct {
    double pos;   // path coordinate in "steps"
    double speed; // steps per second
    int    laps;
} Enemy;

static int    player_x, player_y;
static double player_progress = 0.0;  // path steps forward only
static int    player_laps     = 0;
static int    player_prev_idx = -1;
static double collision_stun  = 0.0;

static Enemy enemies[NUM_ENEMIES];

enum {
    STATE_RUNNING,
    STATE_FINISHED
};
static int   game_state   = STATE_RUNNING;
static int   player_won   = 0;
static double race_start  = 0.0;

/* ---------- Game logic ---------- */

static void init_game(void) {
    init_track();

    // Place player at start line
    if (path_len <= 0) {
        fprintf(stderr, "Path too short.\n");
        exit(1);
    }

    player_x = path_x[0];
    player_y = path_y[0];
    player_progress = 0.0;
    player_laps = 0;
    player_prev_idx = path_index_for(player_x, player_y);
    collision_stun  = 0.0;

    // Enemies: spread them around the circuit, slightly different speeds
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].pos   = (double)((i + 1) * (path_len / NUM_ENEMIES));
        enemies[i].speed = 7.0 + i * 0.7;  // steps per second
        enemies[i].laps  = 0;
    }

    game_state  = STATE_RUNNING;
    player_won  = 0;
    race_start  = now_sec();
}

/* Move player by dx,dy if possible, and update progress/laps */
static void player_move(int dx, int dy) {
    if (collision_stun > 0.0 || game_state != STATE_RUNNING)
        return;

    int nx = player_x + dx;
    int ny = player_y + dy;

    if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT)
        return;

    char cell = track_template[ny][nx];
    if (cell != '.' && cell != 'S')
        return; // walls/grass not allowed

    int idx = path_index_for(nx, ny);
    if (idx < 0)
        return; // Should not happen if track is consistent

    if (player_prev_idx < 0) {
        player_prev_idx = idx;
    } else {
        int diff = idx - player_prev_idx;

        // Wrap diff into [-path_len/2, path_len/2]
        if (diff >  path_len / 2) diff -= path_len;
        if (diff < -path_len / 2) diff += path_len;

        // Only count forward motion along the circuit
        if (diff > 0) {
            player_progress += diff;
            int new_laps = (int)(player_progress / path_len);
            if (new_laps > player_laps)
                player_laps = new_laps;
        }

        player_prev_idx = idx;
    }

    player_x = nx;
    player_y = ny;
}

static int enemy_path_index(int i) {
    int steps = (int)enemies[i].pos;
    int idx = steps % path_len;
    if (idx < 0) idx += path_len;
    return idx;
}

static void update_enemies(double dt) {
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].pos += enemies[i].speed * dt;
        if (enemies[i].pos < 0.0)
            enemies[i].pos = 0.0;
        int steps = (int)enemies[i].pos;
        enemies[i].laps = steps / path_len;
    }
}

static void check_collisions(void) {
    if (game_state != STATE_RUNNING)
        return;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        int idx = enemy_path_index(i);
        int ex = path_x[idx];
        int ey = path_y[idx];

        if (ex == player_x && ey == player_y) {
            if (collision_stun <= 0.0) {
                collision_stun = 1.0; // 1 second stun
            }
        }
    }
}

static void check_victory(void) {
    if (game_state != STATE_RUNNING)
        return;

    if (player_laps >= LAPS_TO_WIN) {
        game_state = STATE_FINISHED;
        player_won = 1;
        return;
    }

    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[i].laps >= LAPS_TO_WIN) {
            game_state = STATE_FINISHED;
            player_won = 0;
            return;
        }
    }
}

/* ---------- Rendering ---------- */

static void render_frame(double elapsed) {
    char screen[HEIGHT][WIDTH];

    // Start from track template
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            screen[y][x] = track_template[y][x];

    // HUD line 0
    char line[256];
    snprintf(line, sizeof(line),
             "Laps: %d/%d   Time: %.1f s   Stun: %s",
             player_laps, LAPS_TO_WIN, elapsed,
             (collision_stun > 0.0 ? "YES" : "NO"));
    int len = (int)strlen(line);
    if (len > WIDTH) len = WIDTH;
    memcpy(screen[0], line, len);

    // HUD line 1 (enemy laps)
    char line2[256];
    char buf[64];
    strcpy(line2, "Enemies laps: ");
    for (int i = 0; i < NUM_ENEMIES; i++) {
        snprintf(buf, sizeof(buf), "%d:%d ", i+1, enemies[i].laps);
        if ((int)strlen(line2) + (int)strlen(buf) < WIDTH)
            strcat(line2, buf);
        else
            break;
    }
    len = (int)strlen(line2);
    if (len > WIDTH) len = WIDTH;
    memcpy(screen[1], line2, len);

    // Bottom instructions
    const char *help = "Controls: WASD to drive, q to quit";
    len = (int)strlen(help);
    if (len > WIDTH) len = WIDTH;
    memcpy(screen[HEIGHT-1], help, len);

    // Place enemies
    for (int i = 0; i < NUM_ENEMIES; i++) {
        int idx = enemy_path_index(i);
        int ex = path_x[idx];
        int ey = path_y[idx];
        if (ex >= 0 && ex < WIDTH && ey >= 0 && ey < HEIGHT) {
            char c = '1' + i;   // '1', '2', '3', ...
            screen[ey][ex] = c;
        }
    }

    // Place player
    if (player_x >= 0 && player_x < WIDTH &&
        player_y >= 0 && player_y < HEIGHT) {
        screen[player_y][player_x] = 'P';
    }

    // Victory / defeat overlay
    if (game_state == STATE_FINISHED) {
        const char *msg  = player_won ? "YOU WIN!"  : "YOU LOSE!";
        const char *msg2 = "Press q to quit.";
        int row = HEIGHT / 2;

        int mlen = (int)strlen(msg);
        int col  = (WIDTH - mlen) / 2;
        if (col < 0) col = 0;
        for (int i = 0; i < mlen && col + i < WIDTH; i++)
            screen[row][col + i] = msg[i];

        int m2len = (int)strlen(msg2);
        int col2  = (WIDTH - m2len) / 2;
        if (col2 < 0) col2 = 0;
        for (int i = 0; i < m2len && col2 + i < WIDTH; i++)
            screen[row + 1][col2 + i] = msg2[i];
    }

    // Move cursor to top-left and draw the whole buffer
    printf("\x1b[H");
    for (int y = 0; y < HEIGHT; y++) {
        fwrite(screen[y], 1, WIDTH, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

/* ---------- Input ---------- */

static int handle_input(void) {
    char ch;
    int quit = 0;
    while (1) {
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n <= 0)
            break;

        if (ch == 'q' || ch == 'Q') {
            quit = 1;
        } else if (game_state == STATE_RUNNING) {
            switch (ch) {
                case 'w': case 'W': player_move( 0, -1); break;
                case 's': case 'S': player_move( 0,  1); break;
                case 'a': case 'A': player_move(-1,  0); break;
                case 'd': case 'D': player_move( 1,  0); break;
                default: break;
            }
        }
    }
    return quit;
}

/* ---------- Main loop ---------- */

int main(void) {
    enable_raw_mode();
    init_game();

    double last = now_sec();
    const double frame_time = 1.0 / 60.0; // ~60 FPS

    int running = 1;
    while (running) {
        double t   = now_sec();
        double dt  = t - last;
        last       = t;
        double elapsed = t - race_start;

        if (handle_input())
            break;

        if (game_state == STATE_RUNNING) {
            if (collision_stun > 0.0) {
                collision_stun -= dt;
                if (collision_stun < 0.0) collision_stun = 0.0;
            }
            update_enemies(dt);
            check_collisions();
            check_victory();
        }

        render_frame(elapsed);

        // Simple frame cap
        double after = now_sec();
        double used  = after - t;
        if (used < frame_time) {
            int us = (int)((frame_time - used) * 1e6);
            if (us > 0)
                usleep(us);
        }
    }

    return 0;
}
