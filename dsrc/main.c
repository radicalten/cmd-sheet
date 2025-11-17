/*
  Galagish - a tiny terminal shmup in single-file C
  - No external libraries (uses only standard/OS APIs)
  - POSIX (Linux/macOS) and Windows support
  - ASCII graphics, ANSI colors (graceful fallback)
  - Arrows/A/D to move, Space to shoot, Q to quit

  Build:
    Linux/macOS: gcc -O2 -std=c99 galagish.c -o galagish
    Windows (MSVC): cl /O2 /TC galagish.c
    Windows (MinGW): gcc -O2 -std=c99 galagish.c -o galagish.exe

  This is an original work inspired by classic arcade shmups (e.g., Galaga).
*/

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#endif

#include <string.h>

/* ---------- Config ---------- */
#define INIT_W 80
#define INIT_H 28

#define MAX_ENEMIES         80
#define MAX_PLAYER_BULLETS  8
#define MAX_ENEMY_BULLETS   48
#define MAX_STARS           120

#define PLAYER_SPEED        28.0f
#define PLAYER_COOLDOWN     0.16f
#define BULLET_SPEED_P      40.0f
#define BULLET_SPEED_E      18.0f

#define BASE_ENEMY_SPEED    6.0f
#define ENEMY_SPACING_X     5
#define ENEMY_SPACING_Y     2

#define INVULN_TIME         2.0f
#define FPS                 60.0

/* ---------- Cross-platform time/input/console ---------- */
typedef struct {
    int left, right, shoot, quit;
} InputState;

static int termW = INIT_W, termH = INIT_H;
static int color_enabled = 1;

#if defined(_WIN32)

/* Windows globals */
static HANDLE hOut, hIn;
static DWORD inModeOrig = 0, outModeOrig = 0;
static int cursor_visible_saved = 1;
static LARGE_INTEGER qpc_freq;

/* timer */
static double now_sec(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)qpc_freq.QuadPart;
}
static void sleep_ms(int ms) { Sleep((DWORD)ms); }

/* console init/shutdown */
static void console_init(void) {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn  = GetStdHandle(STD_INPUT_HANDLE);

    /* Enable VT processing if available */
    GetConsoleMode(hOut, &outModeOrig);
    DWORD outMode = outModeOrig | 0x0004 /*ENABLE_VIRTUAL_TERMINAL_PROCESSING*/;
    if (!SetConsoleMode(hOut, outMode)) {
        color_enabled = 0; /* fallback w/o color */
    }

    GetConsoleMode(hIn, &inModeOrig);
    DWORD inMode = inModeOrig;
    inMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    inMode |= ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(hIn, inMode);

    /* Hide cursor (via WinAPI in case VT not available) */
    CONSOLE_CURSOR_INFO cci;
    GetConsoleCursorInfo(hOut, &cci);
    cursor_visible_saved = cci.bVisible;
    cci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &cci);

    /* Get window size */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        termW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        termH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (termW < 60) termW = 60;
        if (termH < 24) termH = 24;
    }

    QueryPerformanceFrequency(&qpc_freq);

    /* Clear screen and home cursor */
    DWORD written;
    COORD origin = {0,0};
    FillConsoleOutputCharacterA(hOut, ' ', termW * termH, origin, &written);
    SetConsoleCursorPosition(hOut, origin);
    if (color_enabled) printf("\x1b[?25l"); /* Hide cursor via VT too */
    fflush(stdout);
}

static void console_shutdown(void) {
    /* Restore modes */
    if (hOut) SetConsoleMode(hOut, outModeOrig);
    if (hIn)  SetConsoleMode(hIn, inModeOrig);
    /* Show cursor */
    CONSOLE_CURSOR_INFO cci;
    if (hOut && GetConsoleCursorInfo(hOut, &cci)) {
        cci.bVisible = cursor_visible_saved;
        SetConsoleCursorInfo(hOut, &cci);
    }
    if (color_enabled) printf("\x1b[0m\x1b[?25h\n");
    fflush(stdout);
}

/* Input via GetAsyncKeyState */
static void poll_input(InputState *in) {
    SHORT L = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
    SHORT R = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
    SHORT S = GetAsyncKeyState(VK_SPACE);
    SHORT Q = GetAsyncKeyState('Q');
    in->left  = (L & 0x8000) ? 1 : 0;
    in->right = (R & 0x8000) ? 1 : 0;
    in->shoot = (S & 0x8000) ? 1 : 0;
    in->quit  = (Q & 0x8000) ? 1 : 0;
}

#else /* POSIX */

static struct termios term_orig;
static int stdin_flags_orig = -1;

/* timer */
static double now_sec(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
#endif
}
static void sleep_ms(int ms) { usleep(ms * 1000); }

/* console init/shutdown */
static void console_init(void) {
    /* Raw mode, nonblocking stdin */
    struct termios t;
    tcgetattr(0, &term_orig);
    t = term_orig;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    stdin_flags_orig = fcntl(0, F_GETFL);
    fcntl(0, F_SETFL, stdin_flags_orig | O_NONBLOCK);

    /* Get window size */
#if defined(TIOCGWINSZ)
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
        termW = ws.ws_col;
        termH = ws.ws_row;
        if (termW < 60) termW = 60;
        if (termH < 24) termH = 24;
    }
#endif

    /* Clear and hide cursor */
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);
}
static void console_shutdown(void) {
    /* Restore term */
    tcsetattr(0, TCSANOW, &term_orig);
    if (stdin_flags_orig != -1) fcntl(0, F_SETFL, stdin_flags_orig);
    /* Reset color and show cursor */
    printf("\x1b[0m\x1b[?25h\n");
    fflush(stdout);
}

/* Read all available bytes from stdin and parse keys */
static void poll_input(InputState *in) {
    in->left = in->right = in->shoot = in->quit = 0;
    char buf[64];
    ssize_t n = 0;
    while ((n = read(0, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (c == 27) {
                /* Potential escape sequence */
                if (i + 2 < n && buf[i+1] == '[') {
                    char a = buf[i+2];
                    if (a == 'C') in->right = 1;      /* Right arrow */
                    else if (a == 'D') in->left = 1;  /* Left arrow */
                    else if (a == 'A') { /* Up */ }
                    else if (a == 'B') { /* Down */ }
                    i += 2;
                }
            } else if (c == 'a' || c == 'A') in->left = 1;
            else if (c == 'd' || c == 'D') in->right = 1;
            else if (c == ' ') in->shoot = 1;
            else if (c == 'q' || c == 'Q') in->quit = 1;
        }
    }
}

#endif

/* ---------- Rendering buffer ---------- */

static char *g_chars = NULL;
static unsigned char *g_cols = NULL;

enum {
    COL_DEF = 0,
    COL_RED = 31,
    COL_GRN = 32,
    COL_YEL = 33,
    COL_BLU = 34,
    COL_MAG = 35,
    COL_CYN = 36,
    COL_WHT = 37,
    COL_DIM = 90
};

static void buf_init(void) {
    g_chars = (char*)malloc(termW * termH);
    g_cols  = (unsigned char*)malloc(termW * termH);
    memset(g_chars, ' ', termW * termH);
    memset(g_cols, COL_DEF, termW * termH);
}
static void buf_free(void) {
    free(g_chars); g_chars = NULL;
    free(g_cols);  g_cols = NULL;
}
static void buf_clear(void) {
    memset(g_chars, ' ', termW * termH);
    memset(g_cols, COL_DEF, termW * termH);
}
static void putc_xy(int x, int y, char ch, unsigned char col) {
    if (x < 0 || x >= termW || y < 0 || y >= termH) return;
    g_chars[y * termW + x] = ch;
    g_cols[y * termW + x]  = col;
}
static void puts_xy(int x, int y, const char *s, unsigned char col) {
    for (int i = 0; s[i] && x + i < termW; ++i) {
        putc_xy(x + i, y, s[i], col);
    }
}

static void render_flush(void) {
    /* Move cursor home */
    printf("\x1b[H");
    unsigned char curcol = 255; /* invalid */
    for (int y = 0; y < termH; ++y) {
        for (int x = 0; x < termW; ++x) {
            int idx = y * termW + x;
            unsigned char c = g_cols[idx];
            if (color_enabled) {
                if (c != curcol) {
                    if (c == COL_DEF) printf("\x1b[0m");
                    else printf("\x1b[%dm", (int)c);
                    curcol = c;
                }
            }
            putchar(g_chars[idx]);
        }
        if (color_enabled && curcol != COL_DEF) { printf("\x1b[0m"); curcol = COL_DEF; }
        if (y != termH - 1) putchar('\n');
    }
    fflush(stdout);
}

/* ---------- Game state ---------- */

typedef struct {
    float x, y;
    int alive;
    int row, col;
} Enemy;

typedef struct {
    float x, y, vy;
    int active;
    int fromEnemy;
} Bullet;

typedef struct {
    float x, y, speed;
    char ch;
    unsigned char col;
} Star;

static Enemy enemies[MAX_ENEMIES];
static int enemy_count = 0;
static int enemies_alive = 0;
static float enemy_dir = 1.0f;
static float enemy_speed = BASE_ENEMY_SPEED;
static int enemy_rows = 5;
static int enemy_cols = 10;
static float enemy_drop_pending = 0.0f; /* >0 when dropping one row */

static Bullet pbul[MAX_PLAYER_BULLETS];
static Bullet ebul[MAX_ENEMY_BULLETS];

static Star stars[MAX_STARS];

static float player_x = 0.0f;
static int   player_y = 0;
static float shot_cooldown = 0.0f;
static int score = 0;
static int lives = 3;
static int wave = 1;
static float invuln_time = 0.0f;

/* ---------- Helpers ---------- */
static int iround(float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float frand(void) { return (float)rand() / (float)RAND_MAX; }

/* ---------- Spawning ---------- */

static void spawn_stars(void) {
    for (int i = 0; i < MAX_STARS; ++i) {
        stars[i].x = (float)(rand() % termW);
        stars[i].y = (float)(rand() % termH);
        float tier = frand();
        if (tier < 0.5f) { stars[i].speed = 10.0f + frand()*6.0f; stars[i].ch = '.'; stars[i].col = COL_DIM; }
        else if (tier < 0.85f) { stars[i].speed = 16.0f + frand()*8.0f; stars[i].ch = '.'; stars[i].col = COL_DEF; }
        else { stars[i].speed = 22.0f + frand()*10.0f; stars[i].ch = '*'; stars[i].col = COL_WHT; }
    }
}

static void reset_bullets(void) {
    memset(pbul, 0, sizeof(pbul));
    memset(ebul, 0, sizeof(ebul));
}

static void spawn_wave(int w) {
    enemy_rows = 5;
    enemy_cols = 10;
    if (termW < 70) enemy_cols = 8;

    /* Fit formation centered */
    int total_w = (enemy_cols - 1) * ENEMY_SPACING_X + 1;
    int start_x = (termW - total_w) / 2;
    int start_y = 3;

    enemy_speed = BASE_ENEMY_SPEED + (float)(w - 1) * 1.3f;
    enemy_dir = 1.0f;
    enemy_drop_pending = 0.0f;

    enemy_count = enemy_rows * enemy_cols;
    enemies_alive = enemy_count;
    int idx = 0;
    for (int r = 0; r < enemy_rows; ++r) {
        for (int c = 0; c < enemy_cols; ++c) {
            if (idx >= MAX_ENEMIES) break;
            enemies[idx].alive = 1;
            enemies[idx].row = r;
            enemies[idx].col = c;
            enemies[idx].x = (float)(start_x + c * ENEMY_SPACING_X);
            enemies[idx].y = (float)(start_y + r * ENEMY_SPACING_Y);
            idx++;
        }
    }
}

static int count_active_enemy_bullets(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) if (ebul[i].active) n++;
    return n;
}

static void fire_player_bullet(float x, float y) {
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!pbul[i].active) {
            pbul[i].active = 1;
            pbul[i].fromEnemy = 0;
            pbul[i].x = x;
            pbul[i].y = y;
            pbul[i].vy = -BULLET_SPEED_P;
            break;
        }
    }
}

static void fire_enemy_bullet(float x, float y) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!ebul[i].active) {
            ebul[i].active = 1;
            ebul[i].fromEnemy = 1;
            ebul[i].x = x;
            ebul[i].y = y;
            ebul[i].vy = BULLET_SPEED_E;
            break;
        }
    }
}

/* ---------- Update ---------- */

static void update_stars(float dt) {
    for (int i = 0; i < MAX_STARS; ++i) {
        stars[i].y += stars[i].speed * dt;
        if ((int)stars[i].y >= termH) {
            stars[i].y = 0.0f;
            stars[i].x = (float)(rand() % termW);
        }
    }
}

/* Find formation bounds */
static void enemy_bounds(float *minx, float *maxx, float *maxy) {
    float mn = 1e9f, mx = -1e9f, my = -1e9f;
    for (int i = 0; i < enemy_count; ++i) {
        if (!enemies[i].alive) continue;
        if (enemies[i].x < mn) mn = enemies[i].x;
        if (enemies[i].x > mx) mx = enemies[i].x;
        if (enemies[i].y > my) my = enemies[i].y;
    }
    if (minx) *minx = mn;
    if (maxx) *maxx = mx;
    if (maxy) *maxy = my;
}

/* Return an index of a random bottom-most visible enemy in a random column */
static int pick_shooting_enemy(void) {
    if (enemies_alive <= 0) return -1;

    int tries = 8;
    while (tries-- > 0) {
        int c = rand() % enemy_cols;
        /* find lowest in this column */
        int best_idx = -1;
        float best_y = -1.0f;
        for (int i = 0; i < enemy_count; ++i) {
            if (!enemies[i].alive) continue;
            if (enemies[i].col != c) continue;
            if (enemies[i].y > best_y) { best_y = enemies[i].y; best_idx = i; }
        }
        if (best_idx >= 0) return best_idx;
    }
    /* fallback any alive */
    int idx = rand() % enemy_count;
    for (int k = 0; k < enemy_count; ++k) {
        int i = (idx + k) % enemy_count;
        if (enemies[i].alive) return i;
    }
    return -1;
}

static void update_enemies(float dt) {
    if (enemies_alive <= 0) return;

    if (enemy_drop_pending > 0.0f) {
        for (int i = 0; i < enemy_count; ++i) if (enemies[i].alive) enemies[i].y += enemy_drop_pending;
        enemy_drop_pending = 0.0f;
    }

    /* Move horizontally */
    float dx = enemy_dir * enemy_speed * dt;
    for (int i = 0; i < enemy_count; ++i) if (enemies[i].alive) enemies[i].x += dx;

    /* Edge check */
    float mn, mx, my;
    enemy_bounds(&mn, &mx, &my);
    if (mn <= 2.0f && enemy_dir < 0.0f) { enemy_dir = 1.0f; enemy_drop_pending = 1.0f; }
    else if (mx >= (float)(termW - 3) && enemy_dir > 0.0f) { enemy_dir = -1.0f; enemy_drop_pending = 1.0f; }

    /* Enemy firing: rate increases with wave and remaining enemies */
    float base_rate = 1.8f + 0.4f * (float)(wave - 1);
    float alive_factor = (enemies_alive <= 0) ? 0.0f : (0.5f + 0.5f * (float)enemies_alive / (float)(enemy_rows * enemy_cols));
    float rate = base_rate * (0.5f + 0.6f * alive_factor); /* bullets per second */
    float p = rate * dt;
    int cap = 10 + wave * 3;
    if (cap > MAX_ENEMY_BULLETS) cap = MAX_ENEMY_BULLETS;

    if (count_active_enemy_bullets() < cap && frand() < p) {
        int idx = pick_shooting_enemy();
        if (idx >= 0) {
            fire_enemy_bullet(enemies[idx].x, enemies[idx].y + 1.0f);
        }
    }

    /* Check if enemies reach player line */
    if ((int)my >= player_y) {
        lives = 0; /* game over */
    }
}

static void update_bullets(float dt) {
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!pbul[i].active) continue;
        pbul[i].y += pbul[i].vy * dt;
        if ((int)pbul[i].y < 1) pbul[i].active = 0;
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!ebul[i].active) continue;
        ebul[i].y += ebul[i].vy * dt;
        if ((int)ebul[i].y >= termH - 1) ebul[i].active = 0;
    }
}

static void handle_collisions(void) {
    /* Player bullets vs enemies */
    for (int b = 0; b < MAX_PLAYER_BULLETS; ++b) {
        if (!pbul[b].active) continue;
        int bx = iround(pbul[b].x);
        int by = iround(pbul[b].y);
        for (int i = 0; i < enemy_count; ++i) {
            if (!enemies[i].alive) continue;
            int ex = iround(enemies[i].x);
            int ey = iround(enemies[i].y);
            if (bx == ex && by == ey) {
                /* hit */
                enemies[i].alive = 0;
                enemies_alive--;
                pbul[b].active = 0;
                /* score by row (top more) */
                int row = enemies[i].row;
                int base = 10 + (enemy_rows - row) * 10; /* 60..20 */
                score += base;
                break;
            }
        }
    }
    /* Enemy bullets vs player */
    if (invuln_time <= 0.0f) {
        int px = iround(player_x);
        for (int b = 0; b < MAX_ENEMY_BULLETS; ++b) {
            if (!ebul[b].active) continue;
            int bx = iround(ebul[b].x);
            int by = iround(ebul[b].y);
            if (by == player_y && bx == px) {
                /* player hit */
                ebul[b].active = 0;
                lives--;
                invuln_time = INVULN_TIME;
                if (lives < 0) lives = 0;
                break;
            }
        }
    }
}

/* ---------- Draw ---------- */

static void draw_stars(void) {
    for (int i = 0; i < MAX_STARS; ++i) {
        int x = (int)stars[i].x;
        int y = (int)stars[i].y;
        if (y <= 1 || y >= termH - 2) continue; /* keep HUD/controls clean */
        putc_xy(x, y, stars[i].ch, stars[i].col);
    }
}

static void draw_ui(void) {
    char line[256];
    snprintf(line, sizeof(line), "GALAGISH  Score:%06d  Lives:%d  Wave:%d  (A/D or Arrows) Move   Space Shoot   Q Quit", score, lives, wave);
    for (int i = 0; line[i]; ++i) putc_xy(i, 0, line[i], COL_YEL);
    /* Separator */
    for (int x = 0; x < termW; ++x) putc_xy(x, 1, '-', COL_DIM);
}

static void draw_player(double t) {
    /* Blink if invulnerable */
    int visible = 1;
    if (invuln_time > 0.0f) {
        int phase = ((int)(t * 10.0)) & 1;
        visible = phase ? 1 : 0;
    }
    if (visible) {
        int px = iround(player_x);
        putc_xy(px, player_y, '^', COL_GRN);
    }
}

static void draw_enemies(void) {
    for (int i = 0; i < enemy_count; ++i) {
        if (!enemies[i].alive) continue;
        int ex = iround(enemies[i].x);
        int ey = iround(enemies[i].y);
        unsigned char col = (i % 3 == 0) ? COL_CYN : ((i % 3 == 1) ? COL_MAG : COL_RED);
        putc_xy(ex, ey, 'W', col);
    }
}

static void draw_bullets(void) {
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!pbul[i].active) continue;
        putc_xy(iround(pbul[i].x), iround(pbul[i].y), '|', COL_WHT);
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!ebul[i].active) continue;
        putc_xy(iround(ebul[i].x), iround(ebul[i].y), '!', COL_YEL);
    }
}

static void draw_bottom_help(void) {
    const char *msg = "Tip: Focus columns, clear waves to increase score multiplier!";
    int y = termH - 1;
    int x = (termW - (int)strlen(msg)) / 2;
    if (x < 0) x = 0;
    puts_xy(x, y, msg, COL_DIM);
}

/* ---------- Main ---------- */

int main(void) {
    srand((unsigned)time(NULL));
    console_init();
    buf_init();

    /* World setup */
    spawn_stars();
    reset_bullets();
    spawn_wave(wave);

    player_x = (float)(termW / 2);
    player_y = termH - 3;

    double t_prev = now_sec();
    double acc = 0.0;
    const double dt = 1.0 / FPS;

    int running = 1;
    double t_game = 0.0;

    while (running) {
        double t_now = now_sec();
        double frame = t_now - t_prev;
        if (frame > 0.1) frame = 0.1; /* clamp if debugger pause */
        t_prev = t_now;
        acc += frame;
        t_game += frame;

        InputState in = {0,0,0,0};
#if defined(_WIN32)
        poll_input(&in);
#else
        poll_input(&in); /* non-blocking */
#endif
        if (in.quit) running = 0;

        /* Fixed-step updates */
        while (acc >= dt) {
            float fdt = (float)dt;

            /* Player movement */
            float dir = 0.0f;
            if (in.left) dir -= 1.0f;
            if (in.right) dir += 1.0f;
            player_x += dir * PLAYER_SPEED * fdt;
            if (player_x < 2.0f) player_x = 2.0f;
            if (player_x > (float)(termW - 3)) player_x = (float)(termW - 3);

            /* Shooting */
            if (shot_cooldown > 0.0f) shot_cooldown -= fdt;
            if (in.shoot && shot_cooldown <= 0.0f) {
                fire_player_bullet(player_x, (float)player_y - 1.0f);
                shot_cooldown = PLAYER_COOLDOWN;
            }

            /* Stars, enemies, bullets */
            update_stars(fdt);
            update_enemies(fdt);
            update_bullets(fdt);

            /* Collisions */
            handle_collisions();

            /* Invulnerability timer */
            if (invuln_time > 0.0f) invuln_time -= fdt;

            /* Wave cleared? */
            if (enemies_alive <= 0) {
                wave++;
                spawn_wave(wave);
                reset_bullets();
                invuln_time = 0.0f;
            }

            /* Game over */
            if (lives <= 0) {
                running = 0;
                break;
            }

            acc -= dt;
        }

        /* Draw */
        buf_clear();
        draw_stars();
        draw_ui();
        draw_enemies();
        draw_bullets();
        draw_player(t_game);
        draw_bottom_help();
        render_flush();

        /* Frame pacing */
        double after = now_sec();
        double used = after - t_now;
        double target = (1.0 / FPS);
        if (used < target) {
            int ms = (int)((target - used) * 1000.0 - 0.25);
            if (ms > 0) sleep_ms(ms);
        }
    }

    /* Game over screen */
    buf_clear();
    char over1[] = "GAME OVER";
    char over2[128];
    snprintf(over2, sizeof(over2), "Final Score: %d   Waves Cleared: %d", score, wave - 1);
    int ymid = termH / 2;
    int x1 = (termW - (int)strlen(over1)) / 2;
    int x2 = (termW - (int)strlen(over2)) / 2;
    puts_xy(x1 < 0 ? 0 : x1, ymid - 1, over1, COL_RED);
    puts_xy(x2 < 0 ? 0 : x2, ymid + 1, over2, COL_YEL);
    puts_xy(2, termH - 2, "Press Q or Ctrl+C to exit.", COL_DIM);
    render_flush();

    /* Wait briefly so the screen remains visible unless user quits quickly */
#if defined(_WIN32)
    for (;;) {
        InputState in = {0};
        poll_input(&in);
        if (in.quit) break;
        sleep_ms(30);
    }
#else
    /* POSIX: best-effort quit on Q */
    for (;;) {
        InputState in = {0};
        poll_input(&in);
        if (in.quit) break;
        usleep(30000);
    }
#endif

    buf_free();
    console_shutdown();
    return 0;
}
