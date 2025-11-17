// ASCII Shmup - Single-file, no external deps
// Cross-platform terminal game (Windows, Linux, macOS)
// Inspired by classic shmups; original code and assets.
//
// Build:
//   Linux/macOS: gcc -O2 -std=c99 shmup.c -o shmup
//   Windows (MSVC): cl /O2 shmup.c
//   Windows (MinGW): gcc -O2 -std=c99 shmup.c -o shmup.exe
//
// Controls: Left/Right arrows or A/D, Space=Fire, P=Pause, Q=Quit, R=Restart (after game over)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <conio.h>
#else
  #include <unistd.h>
  #include <termios.h>
  #include <fcntl.h>
  #include <sys/ioctl.h>
  #include <sys/time.h>
  #include <errno.h>
#endif

// -------------------- Config --------------------
#define SCREEN_W 80
#define SCREEN_H 28

#define MAX_PBUL 64
#define MAX_EBUL 64
#define ENEMY_ROWS 4
#define ENEMY_COLS 10
#define MAX_ENEMIES (ENEMY_ROWS*ENEMY_COLS)
#define MAX_DIVERS 4
#define STAR_COUNT 80

#define PLAYER_CHAR '^'
#define ENEMY_CHAR 'W'
#define DIVER_CHAR 'V'
#define P_BUL_CHAR '|'
#define E_BUL_CHAR 'v'
#define EXPLO_CHAR '*'

#define HEADER_H 2

// -------------------- Utility --------------------
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static unsigned int xorshift32_state = 0x12345678u;
static unsigned int xrnd() {
    // xorshift32
    unsigned int x = xorshift32_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift32_state = x;
    return x;
}
static int irand(int lo, int hi) { // inclusive
    if (hi <= lo) return lo;
    return (int)(lo + (xrnd() % (unsigned)(hi - lo + 1)));
}

// -------------------- Platform Abstraction --------------------
static void out_flush() { fflush(stdout); }

static void hide_cursor() { fputs("\x1b[?25l", stdout); }
static void show_cursor() { fputs("\x1b[?25h", stdout); }
static void clear_screen_full() { fputs("\x1b[2J", stdout); }
static void cursor_home() { fputs("\x1b[H", stdout); }

#ifdef _WIN32
static HANDLE g_hOut;
static DWORD g_origOutMode = 0;
static int platform_initialized = 0;

static void platform_init() {
    if (platform_initialized) return;
    platform_initialized = 1;
    // Init RNG seed
    xorshift32_state = (unsigned)time(NULL) ^ 0xA5A5A5A5u;

    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(g_hOut, &mode)) {
            g_origOutMode = mode;
            mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(g_hOut, mode);
        }
    }

    // No need to set input raw; we'll use _kbhit/_getch
    // Hide cursor and clear
    hide_cursor();
    clear_screen_full();
    cursor_home();
    // Buffering to speed prints
    setvbuf(stdout, NULL, _IOFBF, 0);
}

static void platform_shutdown() {
    if (!platform_initialized) return;
    platform_initialized = 0;
    if (g_hOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(g_hOut, g_origOutMode);
    }
    show_cursor();
}

static void sleep_ms(int ms) { Sleep(ms); }

static double now_sec() {
    static LARGE_INTEGER freq = {0};
    static LARGE_INTEGER start = {0};
    static int inited = 0;
    LARGE_INTEGER t;
    if (!inited) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        inited = 1;
    }
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - start.QuadPart) / (double)freq.QuadPart;
}

static int read_key_nonblock() {
    if (_kbhit()) {
        int c = _getch();
        // Arrow keys: prefix 0 or 224, then code
        if (c == 0 || c == 224) {
            int c2 = _getch();
            if (c2 == 75) return 1000 + 'L'; // Left
            if (c2 == 77) return 1000 + 'R'; // Right
            if (c2 == 72) return 1000 + 'U'; // Up
            if (c2 == 80) return 1000 + 'D'; // Down
            return -1;
        }
        return c;
    }
    return -1;
}

#else // POSIX
static struct termios g_origTerm;
static int term_raw_enabled = 0;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void enable_raw() {
    if (term_raw_enabled) return;
    term_raw_enabled = 1;
    tcgetattr(STDIN_FILENO, &g_origTerm);
    struct termios raw = g_origTerm;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    set_nonblocking(STDIN_FILENO);
}

static void disable_raw() {
    if (!term_raw_enabled) return;
    term_raw_enabled = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_origTerm);
}

static void platform_init() {
    xorshift32_state = (unsigned)time(NULL) ^ 0x5A5A5A5Au;
    enable_raw();
    hide_cursor();
    clear_screen_full();
    cursor_home();
    setvbuf(stdout, NULL, _IOFBF, 0);
}

static void platform_shutdown() {
    show_cursor();
    disable_raw();
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int read_key_nonblock() {
    unsigned char c;
    int n = (int)read(STDIN_FILENO, &c, 1);
    if (n <= 0) return -1;
    if (c == 0x1B) { // ESC sequence
        unsigned char buf[2];
        // Try to read next two bytes if present
        int n1 = (int)read(STDIN_FILENO, &buf[0], 1);
        if (n1 <= 0) return 27;
        if (buf[0] != '[') return 27;
        int n2 = (int)read(STDIN_FILENO, &buf[1], 1);
        if (n2 <= 0) return 27;
        if (buf[1] == 'D') return 1000 + 'L'; // Left
        if (buf[1] == 'C') return 1000 + 'R'; // Right
        if (buf[1] == 'A') return 1000 + 'U'; // Up
        if (buf[1] == 'B') return 1000 + 'D'; // Down
        return -1;
    }
    return (int)c;
}
#endif

// -------------------- Game Types --------------------
typedef struct {
    int x, y;
    int alive;
    int dy;
    char ch;
} Bullet;

typedef struct {
    float x, y;
    int alive;
    int mode;    // 0=formation, 1=diving, 2=returning
    float vx, vy;
    int row, col; // formation cell
    int expl_timer; // explosion frames
} Enemy;

typedef struct {
    float x, y;
    float speed;
    char ch;
} Star;

typedef struct {
    int left, right, fire, quit, pause, restart;
} Input;

// -------------------- Game State --------------------
static char screen[SCREEN_H][SCREEN_W + 1];
static int running = 1;

static int player_x = SCREEN_W / 2;
static int player_y = SCREEN_H - 2;
static int fire_cooldown = 0;
static int player_inv = 0; // frames of invincibility after hit

static Bullet pbul[MAX_PBUL];
static Bullet ebul[MAX_EBUL];

static Enemy enemies[MAX_ENEMIES];
static int enemies_alive = 0;

static float form_x = 10.0f;
static float form_y = 4.0f;
static int form_dir = 1; // 1 right, -1 left
static float form_speed = 0.25f;

static int wave = 1;
static int score = 0;
static int lives = 3;

static Star stars[STAR_COUNT];

static int paused = 0;
static int game_over = 0;

// -------------------- Helpers --------------------
static void draw_text(int x, int y, const char* s) {
    if (y < 0 || y >= SCREEN_H) return;
    for (int i = 0; s[i] && x + i < SCREEN_W; ++i) {
        if (x + i >= 0) screen[y][x + i] = s[i];
    }
}

static void clear_screen_buffer() {
    for (int y = 0; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) screen[y][x] = ' ';
        screen[y][SCREEN_W] = '\0';
    }
}

static void present_screen() {
    cursor_home();
    for (int y = 0; y < SCREEN_H; ++y) {
        fputs(screen[y], stdout);
        fputc('\n', stdout);
    }
    out_flush();
}

static void init_stars() {
    for (int i = 0; i < STAR_COUNT; ++i) {
        stars[i].x = (float)irand(0, SCREEN_W - 1);
        stars[i].y = (float)irand(HEADER_H, SCREEN_H - 2);
        stars[i].speed = 0.1f + 0.3f * ((xrnd() % 100) / 100.0f);
        stars[i].ch = (xrnd() % 4 == 0) ? '*' : '.';
    }
}

static void update_stars() {
    for (int i = 0; i < STAR_COUNT; ++i) {
        stars[i].y += stars[i].speed;
        if ((int)stars[i].y >= SCREEN_H - 1) {
            stars[i].y = (float)HEADER_H;
            stars[i].x = (float)irand(0, SCREEN_W - 1);
            stars[i].speed = 0.1f + 0.3f * ((xrnd() % 100) / 100.0f);
            stars[i].ch = (xrnd() % 5 == 0) ? '*' : '.';
        }
    }
}

static void draw_stars() {
    for (int i = 0; i < STAR_COUNT; ++i) {
        int x = (int)stars[i].x;
        int y = (int)stars[i].y;
        if (x >= 0 && x < SCREEN_W && y >= HEADER_H && y < SCREEN_H - 1) {
            if (screen[y][x] == ' ') screen[y][x] = stars[i].ch;
        }
    }
}

static void reset_bullets() {
    for (int i = 0; i < MAX_PBUL; ++i) pbul[i].alive = 0;
    for (int i = 0; i < MAX_EBUL; ++i) ebul[i].alive = 0;
}

static void spawn_pbullet(int x, int y) {
    for (int i = 0; i < MAX_PBUL; ++i) {
        if (!pbul[i].alive) {
            pbul[i].alive = 1;
            pbul[i].x = x;
            pbul[i].y = y;
            pbul[i].dy = -1;
            pbul[i].ch = P_BUL_CHAR;
            break;
        }
    }
}

static void spawn_ebullet(int x, int y) {
    for (int i = 0; i < MAX_EBUL; ++i) {
        if (!ebul[i].alive) {
            ebul[i].alive = 1;
            ebul[i].x = x;
            ebul[i].y = y;
            ebul[i].dy = 1;
            ebul[i].ch = E_BUL_CHAR;
            break;
        }
    }
}

static void init_enemies() {
    enemies_alive = 0;
    for (int r = 0; r < ENEMY_ROWS; ++r) {
        for (int c = 0; c < ENEMY_COLS; ++c) {
            int idx = r * ENEMY_COLS + c;
            Enemy *e = &enemies[idx];
            e->alive = 1;
            e->mode = 0;
            e->row = r;
            e->col = c;
            e->x = 0;
            e->y = 0;
            e->vx = 0;
            e->vy = 0;
            e->expl_timer = 0;
            enemies_alive++;
        }
    }
    // formation start
    form_x = 8.0f;
    form_y = 4.0f;
    form_dir = 1;
    form_speed = 0.2f + 0.03f * (float)(wave - 1);
}

static void next_wave() {
    wave++;
    init_enemies();
}

static void reset_game() {
    score = 0;
    lives = 3;
    wave = 1;
    player_x = SCREEN_W / 2;
    player_y = SCREEN_H - 2;
    fire_cooldown = 0;
    player_inv = 0;
    paused = 0;
    game_over = 0;
    init_stars();
    reset_bullets();
    init_enemies();
}

// compute formation cell position on screen for a given row/col
static void formation_cell_pos(int row, int col, int* ox, int* oy) {
    int spacing_x = 6;
    int spacing_y = 2;
    int left = (int)form_x;
    int top = (int)form_y;
    *ox = left + col * spacing_x;
    *oy = top + row * spacing_y;
}

static void try_enemy_fire() {
    // Randomly choose an alive enemy to shoot
    // Probability increases with wave slightly
    int chance = 12 + (wave * 2); // out of 1000
    if ((xrnd() % 1000) < chance) {
        int attempts = 20;
        while (attempts-- > 0) {
            int idx = irand(0, MAX_ENEMIES - 1);
            Enemy* e = &enemies[idx];
            if (!e->alive) continue;
            int ex, ey;
            if (e->mode == 0) {
                formation_cell_pos(e->row, e->col, &ex, &ey);
            } else {
                ex = (int)e->x; ey = (int)e->y;
            }
            if (ey >= HEADER_H && ey < SCREEN_H - 2) {
                spawn_ebullet(ex, ey + 1);
                break;
            }
        }
    }
}

static void maybe_launch_diver() {
    // Occasionally pick a top-row or side enemy to dive
    if ((xrnd() % 1000) < 8 + wave) {
        // pick a random alive enemy in formation mode
        int indices[MAX_ENEMIES];
        int n = 0;
        for (int i = 0; i < MAX_ENEMIES; ++i) {
            if (enemies[i].alive && enemies[i].mode == 0) {
                indices[n++] = i;
            }
        }
        if (n > 0) {
            int idx = indices[irand(0, n - 1)];
            Enemy* e = &enemies[idx];
            // set to diving
            int ex, ey;
            formation_cell_pos(e->row, e->col, &ex, &ey);
            e->x = (float)ex;
            e->y = (float)ey;
            e->mode = 1;
            // velocity toward player with downward bias
            float dx = (float)player_x - e->x;
            float sgn = (dx > 0) ? 1.0f : (dx < 0 ? -1.0f : 0.0f);
            e->vx = sgn * (0.15f + 0.03f * (float)wave);
            e->vy = 0.25f + 0.04f * (float)wave;
        }
    }
}

static void kill_enemy(Enemy* e) {
    if (e->alive) {
        e->alive = 0;
        e->mode = 0;
        e->expl_timer = 4; // draw a short explosion at their last spot
        enemies_alive--;
        score += 50 + 10 * wave;
    }
}

static void update_enemies() {
    // Update formation movement
    int spacing_x = 6;
    int spacing_y = 2;

    // Determine leftmost and rightmost cell for current alive enemies
    int minc = ENEMY_COLS, maxc = -1;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive || enemies[i].mode != 0) continue;
        if (enemies[i].col < minc) minc = enemies[i].col;
        if (enemies[i].col > maxc) maxc = enemies[i].col;
    }
    if (minc == ENEMY_COLS) { // no formation enemies, still move anchor gently
        form_x += form_dir * form_speed;
    } else {
        float left_edge = form_x + minc * spacing_x;
        float right_edge = form_x + maxc * spacing_x;
        if (left_edge <= 1) form_dir = 1;
        if (right_edge >= SCREEN_W - 2) form_dir = -1;
        form_x += form_dir * form_speed;
    }

    // Slight vertical bob
    form_y += (xrnd() % 50 == 0) ? 1.0f : 0.0f;
    if (form_y > 8.0f) form_y = 4.0f;

    // Launch divers and try firing
    maybe_launch_diver();
    try_enemy_fire();

    // Update diving/returning enemies
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy* e = &enemies[i];
        if (!e->alive) continue;
        if (e->mode == 1) { // diving
            // steer slightly toward player
            float target_vx = (player_x > (int)e->x) ? +fabsf(e->vx) : (player_x < (int)e->x) ? -fabsf(e->vx) : e->vx;
            // Smooth adjust
            e->vx = (e->vx * 4.0f + target_vx) / 5.0f;
            e->x += e->vx;
            e->y += e->vy;

            // occasional shot while diving
            if ((xrnd() % 100) < 2) {
                spawn_ebullet((int)e->x, (int)e->y + 1);
            }

            // if off bottom, return to formation
            if ((int)e->y >= SCREEN_H - 2) {
                e->mode = 2; // returning
            }
        } else if (e->mode == 2) { // returning to formation cell
            int tx, ty;
            formation_cell_pos(e->row, e->col, &tx, &ty);
            float dx = (float)tx - e->x;
            float dy = (float)ty - e->y;
            float ax = (dx > 0.f) ? 0.3f : (dx < 0.f ? -0.3f : 0.0f);
            float ay = (dy > 0.f) ? 0.2f : (dy < 0.f ? -0.2f : 0.0f);
            e->x += ax;
            e->y += ay;
            if (abs((int)(e->x - tx)) <= 1 && abs((int)(e->y - ty)) <= 1) {
                e->mode = 0;
            }
        }
    }
}

static void update_bullets() {
    // Player bullets
    for (int i = 0; i < MAX_PBUL; ++i) {
        if (!pbul[i].alive) continue;
        pbul[i].y += pbul[i].dy;
        if (pbul[i].y < HEADER_H) { pbul[i].alive = 0; continue; }
        // collision with enemies
        for (int j = 0; j < MAX_ENEMIES; ++j) {
            Enemy* e = &enemies[j];
            if (!e->alive) continue;
            int ex, ey;
            if (e->mode == 0) {
                formation_cell_pos(e->row, e->col, &ex, &ey);
            } else {
                ex = (int)e->x; ey = (int)e->y;
            }
            if (pbul[i].x == ex && pbul[i].y == ey) {
                pbul[i].alive = 0;
                kill_enemy(e);
                break;
            }
        }
    }

    // Enemy bullets
    for (int i = 0; i < MAX_EBUL; ++i) {
        if (!ebul[i].alive) continue;
        ebul[i].y += ebul[i].dy;
        if (ebul[i].y >= SCREEN_H - 1) { ebul[i].alive = 0; continue; }
        // collision with player
        if (player_inv == 0 && ebul[i].x == player_x && ebul[i].y == player_y) {
            ebul[i].alive = 0;
            if (lives > 0) lives--;
            player_inv = 40; // ~2/3 sec invincibility at 60fps
            if (lives <= 0) {
                game_over = 1;
            }
        }
    }
}

static void update_player(const Input* in) {
    if (in->left) player_x -= 1;
    if (in->right) player_x += 1;
    player_x = clampi(player_x, 1, SCREEN_W - 2);

    if (fire_cooldown > 0) fire_cooldown--;
    if (in->fire && fire_cooldown == 0) {
        spawn_pbullet(player_x, player_y - 1);
        fire_cooldown = 6; // small cooldown
    }
}

static void draw_header() {
    char buf[SCREEN_W + 1];
    snprintf(buf, sizeof(buf), " SCORE: %06d   LIVES: %d   WAVE: %d   [A/D or ←/→] Move  [Space] Fire  [P] Pause  [Q] Quit", score, lives, wave);
    draw_text(0, 0, buf);
    for (int x = 0; x < SCREEN_W; ++x) screen[1][x] = '-';
}

static void draw_game() {
    // stars first
    draw_stars();

    // draw enemies
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy* e = &enemies[i];
        int ex, ey;
        if (e->alive) {
            if (e->mode == 0) {
                formation_cell_pos(e->row, e->col, &ex, &ey);
                if (ex >= 0 && ex < SCREEN_W && ey >= HEADER_H && ey < SCREEN_H - 1)
                    screen[ey][ex] = ENEMY_CHAR;
            } else if (e->mode == 1 || e->mode == 2) {
                ex = (int)e->x; ey = (int)e->y;
                if (ex >= 0 && ex < SCREEN_W && ey >= HEADER_H && ey < SCREEN_H - 1)
                    screen[ey][ex] = DIVER_CHAR;
            }
        } else if (e->expl_timer > 0) {
            ex = (int)e->x; ey = (int)e->y;
            e->expl_timer--;
            if (ex >= 0 && ex < SCREEN_W && ey >= HEADER_H && ey < SCREEN_H - 1)
                screen[ey][ex] = EXPLO_CHAR;
        }
    }

    // draw bullets
    for (int i = 0; i < MAX_PBUL; ++i)
        if (pbul[i].alive && pbul[i].y >= HEADER_H && pbul[i].y < SCREEN_H)
            screen[pbul[i].y][pbul[i].x] = pbul[i].ch;
    for (int i = 0; i < MAX_EBUL; ++i)
        if (ebul[i].alive && ebul[i].y >= HEADER_H && ebul[i].y < SCREEN_H)
            screen[ebul[i].y][ebul[i].x] = ebul[i].ch;

    // draw player
    if ((player_inv / 4) % 2 == 0 || game_over) { // blink while invincible
        screen[player_y][player_x] = PLAYER_CHAR;
    }
}

static void draw_overlay() {
    if (paused && !game_over) {
        const char* t = "PAUSED - Press P to resume";
        draw_text((SCREEN_W - (int)strlen(t)) / 2, SCREEN_H / 2, t);
    }
    if (game_over) {
        const char* t1 = "GAME OVER";
        const char* t2 = "Press R to Restart or Q to Quit";
        draw_text((SCREEN_W - (int)strlen(t1)) / 2, SCREEN_H / 2 - 1, t1);
        draw_text((SCREEN_W - (int)strlen(t2)) / 2, SCREEN_H / 2 + 1, t2);
    } else if (enemies_alive == 0) {
        const char* t1 = "WAVE CLEARED!";
        const char* t2 = "Get ready...";
        draw_text((SCREEN_W - (int)strlen(t1)) / 2, SCREEN_H / 2 - 1, t1);
        draw_text((SCREEN_W - (int)strlen(t2)) / 2, SCREEN_H / 2 + 1, t2);
    }
}

static void poll_input(Input* in, Input* prev) {
    memset(in, 0, sizeof(*in));
    int c;
    while ((c = read_key_nonblock()) != -1) {
        if (c == 'a' || c == 'A' || c == (1000 + 'L')) in->left = 1;
        if (c == 'd' || c == 'D' || c == (1000 + 'R')) in->right = 1;
        if (c == ' ') in->fire = 1;
        if (c == 'q' || c == 'Q') in->quit = 1;
        if (c == 'p' || c == 'P') in->pause = 1;
        if (c == 'r' || c == 'R') in->restart = 1;
    }
    // allow held keys for move/fire, but treat pause as edge-triggered toggle
    if (in->pause && !prev->pause) {
        paused = !paused;
    }
}

// -------------------- Main Loop --------------------
int main(void) {
    atexit(platform_shutdown);
    platform_init();

    // Intro
    clear_screen_full();
    cursor_home();

    init_stars();
    reset_bullets();
    init_enemies();

    double t_prev = now_sec();
    double accumulator = 0.0;
    const double dt = 1.0 / 60.0; // 60 FPS
    Input in = {0}, prev = {0};
    int post_wave_cooldown = 0;

    while (running) {
        // Timing
        double t_now = now_sec();
        double frame = t_now - t_prev;
        if (frame > 0.1) frame = 0.1; // clamp
        t_prev = t_now;
        accumulator += frame;

        // Input
        poll_input(&in, &prev);
        if (in.quit) break;

        // Update at fixed rate
        while (accumulator >= dt) {
            accumulator -= dt;

            if (!paused && !game_over) {
                update_stars();
                if (player_inv > 0) player_inv--;

                if (enemies_alive == 0) {
                    if (post_wave_cooldown == 0) post_wave_cooldown = 60;
                    else {
                        post_wave_cooldown--;
                        if (post_wave_cooldown == 0) next_wave();
                    }
                } else {
                    update_player(&in);
                    update_bullets();
                    update_enemies();
                }
            } else if (game_over) {
                // Allow restart
                if (in.restart) {
                    reset_game();
                }
            }
        }

        // Render
        clear_screen_buffer();
        draw_header();
        draw_game();
        draw_overlay();
        present_screen();

        prev = in;
        sleep_ms(6); // small sleep to reduce CPU usage
    }

    // Cleanup
    platform_shutdown();
    return 0;
}
