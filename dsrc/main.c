// outrun.c - tiny pseudo-3D "drive into the screen" ASCII/ANSI racer
// No external dependencies; single-file; cross-platform (Windows/macOS/Linux).
// Controls: Arrow keys or WASD to steer/accelerate, Q to quit.

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ANSI helpers
static inline void ansi_home(void){ printf("\x1b[H"); }
static inline void ansi_clear(void){ printf("\x1b[2J"); }
static inline void ansi_hide_cursor(void){ printf("\x1b[?25l"); }
static inline void ansi_show_cursor(void){ printf("\x1b[?25h"); }
static inline void ansi_reset(void){ printf("\x1b[0m"); }
static inline void ansi_move(int row, int col){ printf("\x1b[%d;%dH", row, col); }
static inline void ansi_bg(int code){ printf("\x1b[%dm", code); }   // 40-47 or 100-107
static inline void ansi_fg(int code){ printf("\x1b[%dm", code); }   // 30-37 or 90-97

// Timing
static void sleep_ms(int ms) {
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

typedef struct {
    int cols;
    int rows;
} TermSize;

static TermSize get_term_size(void) {
    TermSize s = {80, 28};
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(h, &info)) {
        s.cols = info.srWindow.Right - info.srWindow.Left + 1;
        s.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
#else
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) s.cols = ws.ws_col;
        if (ws.ws_row > 0) s.rows = ws.ws_row;
    }
#endif
    return s;
}

// Input (non-blocking)
#if defined(_WIN32)

static int win_raw_enabled = 0;
static DWORD win_old_in_mode = 0;
static DWORD win_old_out_mode = 0;

static void enable_win_raw(void){
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hin, &win_old_in_mode);
    GetConsoleMode(hout, &win_old_out_mode);
    DWORD inMode = win_old_in_mode | ENABLE_EXTENDED_FLAGS;
    inMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hin, inMode);
    win_raw_enabled = 1;
}
static void disable_win_raw(void){
    if (win_raw_enabled){
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleMode(hin, win_old_in_mode);
        SetConsoleMode(hout, win_old_out_mode);
        win_raw_enabled = 0;
    }
}

static int poll_input(int *dx, int *ds, int *quit){
    *dx = 0; *ds = 0; *quit = 0;
    while (_kbhit()){
        int c = _getch();
        if (c == 0 || c == 224) {
            int c2 = _getch();
            if (c2 == 75) *dx = -1;      // left
            else if (c2 == 77) *dx = 1;  // right
            else if (c2 == 72) *ds = 1;  // up
            else if (c2 == 80) *ds = -1; // down
        } else {
            if (c=='a' || c=='A') *dx = -1;
            else if (c=='d' || c=='D') *dx = 1;
            else if (c=='w' || c=='W') *ds = 1;
            else if (c=='s' || c=='S') *ds = -1;
            else if (c=='q' || c=='Q' || c==27) *quit = 1;
        }
    }
    return 1;
}

BOOL WINAPI ctrl_handler(DWORD type){
    (void)type;
    ansi_show_cursor();
    ansi_reset();
    disable_win_raw();
    return FALSE;
}

#else

static struct termios orig_term;
static int raw_enabled = 0;

static void disable_raw(void){
    if (raw_enabled) {
        tcsetattr(0, TCSANOW, &orig_term);
        raw_enabled = 0;
    }
}
static void enable_raw(void){
    struct termios t;
    tcgetattr(0, &orig_term);
    t = orig_term;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);
    raw_enabled = 1;
}
static void on_sigint(int sig){
    (void)sig;
    ansi_show_cursor();
    ansi_reset();
    disable_raw();
    // Also ensure cursor lands after the screen
    ansi_move(9999, 1);
    printf("\n");
    exit(0);
}

static int poll_input(int *dx, int *ds, int *quit){
    *dx = 0; *ds = 0; *quit = 0;
    unsigned char buf[8];
    int n = read(0, buf, sizeof(buf));
    for (int i=0; i<n; ++i){
        unsigned char c = buf[i];
        if (c == 0x1B){ // ESC sequence
            // Try to parse arrows: ESC [ A/B/C/D
            if (i+2 < n && buf[i+1]=='['){
                unsigned char k = buf[i+2];
                if (k=='A') *ds = 1;       // up
                else if (k=='B') *ds = -1; // down
                else if (k=='C') *dx = 1;  // right
                else if (k=='D') *dx = -1; // left
                i += 2;
            }
        } else {
            if (c=='a' || c=='A') *dx = -1;
            else if (c=='d' || c=='D') *dx = 1;
            else if (c=='w' || c=='W') *ds = 1;
            else if (c=='s' || c=='S') *ds = -1;
            else if (c=='q' || c=='Q') *quit = 1;
        }
    }
    return 1;
}
#endif

// Clamp helper
static double clampd(double x, double a, double b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}
static int clampi(int x, int a, int b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

// Rendering funcs
static void draw_sky_line(int W) {
    ansi_bg(44); // blue sky
    for (int x=0; x<W; ++x) putchar(' ');
    ansi_reset();
    putchar('\n');
}

static void draw_line(int W, int row, int horizon, double zpos, double playerX, double speed) {
    // Perspective parameter p in [0..1] from horizon to bottom
    if (row < horizon) {
        draw_sky_line(W);
        return;
    }

    double p = (double)(row - horizon) / (double)(W + (horizon > 0 ? (horizon) : 1));
    if (p < 0) p = 0;
    if (p > 1) p = 1;

    // Road geometry
    // worldZ estimate associated with this screen row (bigger when closer to bottom)
    double worldZ = zpos + (200.0 / (p + 0.01));

    // Curvature: sum of gentle sinusoids
    double curve = sin(worldZ * 0.012) * 0.9 + sin(worldZ * 0.004 + 1.7) * 0.6;
    // Center shift on screen (amplify near bottom)
    int center = W/2 + (int)((curve * (W * 0.18) + playerX * (W * 0.25)) * p);

    // Width of half road scales ~p^2 for nicer perspective
    int half = (int)(2 + (W * 0.45) * (p*p));
    // Rumble strip width
    int rumble = clampi(1 + (int)(p * 8), 1, 6);

    // Dashing pattern based on distance
    int dash = (((int)(worldZ / 6.0)) & 1) == 0;     // lane dash toggle
    int rumbleAlt = (((int)(worldZ / 3.0)) & 1);     // rumble red/white alternation
    int grassAlt = (((int)(worldZ / 9.0)) & 1);      // grass light/dark

    int left = center - half;
    int right = center + half;

    // Lane marker width (1-2 chars)
    int lanew = (p > 0.5) ? 2 : 1;

    // Build line by regions: grass | rumble | road | rumble | grass
    for (int x=0; x<W; ) {
        if (x < left) {
            // Grass
            int segEnd = left;
            if (segEnd > W) segEnd = W;
            ansi_bg(grassAlt ? 42 : 102); // green vs bright green
            while (x < segEnd) { putchar(' '); x++; }
            ansi_reset();
        } else if (x < left + rumble) {
            // Left rumble strip
            int segEnd = left + rumble;
            if (segEnd > W) segEnd = W;
            ansi_bg(rumbleAlt ? 41 : 47); // red/white
            while (x < segEnd) { putchar(' '); x++; }
            ansi_reset();
        } else if (x < right - rumble) {
            // Road interior
            int segEnd = right - rumble;
            if (segEnd > W) segEnd = W;
            while (x < segEnd) {
                // Lane marker around center
                int onLane = dash && (abs(x - center) <= lanew);
                if (onLane) {
                    ansi_bg(107); // white lane
                    putchar(' ');
                    ansi_reset();
                } else {
                    ansi_bg(100); // bright black (dark gray) road
                    putchar(' ');
                    ansi_reset();
                }
                x++;
            }
        } else if (x < right) {
            // Right rumble
            int segEnd = right;
            if (segEnd > W) segEnd = W;
            ansi_bg(rumbleAlt ? 47 : 41); // white/red inverse
            while (x < segEnd) { putchar(' '); x++; }
            ansi_reset();
        } else {
            // Right grass
            int segEnd = W;
            ansi_bg(grassAlt ? 102 : 42);
            while (x < segEnd) { putchar(' '); x++; }
            ansi_reset();
        }
    }
    putchar('\n');
}

static void draw_car(int W, int H, int horizon, double playerX) {
    // Place car near bottom, centered with playerX
    const char *car1 = "   __   ";
    const char *car2 = " _/||\\_ ";
    const char *car3 = "o_/  \\_o";

    int carW = (int)strlen(car1);
    int row = H - 3; // 3 lines tall
    if (row <= horizon + 2) return;

    int center = W/2 + (int)(playerX * (W * 0.25));
    int col = center - carW/2;
    col = clampi(col, 1, W - carW + 1);

    // Draw with red body, bright details
    ansi_move(row, col);     ansi_fg(91); printf("%s", car1); ansi_reset();
    ansi_move(row+1, col);   ansi_fg(91); printf("%s", car2); ansi_reset();
    ansi_move(row+2, col);   ansi_fg(97); printf("%s", car3); ansi_reset();
}

static void draw_hud(int W, int H, double speed, double dist) {
    ansi_move(1, 2);
    ansi_fg(97); printf("SPEED: "); ansi_fg(93); printf("%3.0f", speed * 100); ansi_fg(97); printf("   ");
    ansi_fg(97); printf("DIST: "); ansi_fg(96); printf("%6.1f   ", dist);
    ansi_reset();

    // If terminal is very small, warn
    if (W < 60 || H < 20) {
        ansi_move(2, 2);
        ansi_fg(93); printf("Enlarge terminal for better view (>= 80x28).");
        ansi_reset();
    }
}

int main(void){
    TermSize ts = get_term_size();
    int W = ts.cols;
    int H = ts.rows;

    if (W < 60) W = 60;
    if (H < 24) H = 24;

#if defined(_WIN32)
    enable_win_raw();
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
    enable_raw();
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);
#endif

    ansi_hide_cursor();
    ansi_clear();
    ansi_home();

    // Game state
    double playerX = 0.0;  // -1 left .. +1 right
    double speed = 0.60;   // units per frame (tuned for ~60fps)
    double dist = 0.0;     // distance traveled
    double zpos = 0.0;     // world position (for road animation)
    const int target_ms = 16; // ~60 FPS

    // Horizon roughly at 1/3 of screen
    int horizon = H / 3;

    int running = 1;

    while (running) {
        // Input
        int dx=0, ds=0, quit=0;
        poll_input(&dx, &ds, &quit);
        if (quit) break;

        // Update
        playerX += dx * 0.06;
        playerX = clampd(playerX, -1.2, 1.2);
        speed += ds * 0.03;
        speed = clampd(speed, 0.2, 1.2);

        // Simulate car being pulled toward road center on sharp curves (drift)
        // This gives a bit of "feel" without collision.
        double roadCurveAtCam = sin((zpos + 200.0) * 0.012) * 0.9 + sin((zpos + 200.0) * 0.004 + 1.7) * 0.6;
        playerX -= roadCurveAtCam * 0.008; // mild pull to center line

        zpos += speed * 2.2;
        dist += speed * 0.7;

        // Render
        ansi_home();
        // Top HUD line is separate (we'll overwrite first line below)
        draw_hud(W, H, speed, dist);

        // Draw content from line 2 downwards
        for (int r = 2; r <= H; ++r) {
            if (r-1 == horizon) {
                // ensure a strong horizon
                ansi_bg(44);
                for (int i=0; i<W; ++i) putchar(' ');
                ansi_reset();
                putchar('\n');
            } else {
                draw_line(W, r-1, horizon, zpos, playerX, speed);
            }
        }

        // Car overlay
        draw_car(W, H, horizon, playerX);

        fflush(stdout);
        sleep_ms(target_ms);
    }

    // Cleanup
    ansi_show_cursor();
    ansi_reset();
#if defined(_WIN32)
    disable_win_raw();
#else
    disable_raw();
#endif
    ansi_move(H, 1);
    printf("\n");
    return 0;
}
