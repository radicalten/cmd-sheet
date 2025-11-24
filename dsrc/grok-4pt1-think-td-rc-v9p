// Single-file terminal racing game - no dependencies beyond standard POSIX
// Compile: gcc -o racer racer.c -lm
// Run: ./racer

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>

#define WIDTH 80
#define HEIGHT 25
#define NUM_ENEMIES 3

char background[HEIGHT][WIDTH + 1];
struct termios oldt;

struct Player {
    double x, y, angle, speed;
    int laps, cp;
    int prev_px, prev_py;
} player;

struct Enemy {
    double t;
    double speed_inc;
    int laps;
    int prev_px, prev_py;
} enemies[NUM_ENEMIES];

const char *enemy_colors[3] = { "\033[34m", "\033[33m", "\033[32m" }; // blue, yellow, green

void init_background(void) {
    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    int oa = 36, ob = 11;   // outer
    int ia = 22, ib = 6;    // inner

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            background[y][x] = '#';
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) {
            double do_outer = pow((x - cx) / (double)oa, 2) + pow((y - cy) / (double)ob, 2);
            double do_inner = pow((x - cx) / (double)ia, 2) + pow((y - cy) / (double)ib, 2);
            if (do_outer <= 1.0 && do_inner >= 1.0)
                background[y][x] = ' ';
        }
    for (int y = 0; y < HEIGHT; y++) background[y][WIDTH] = '\0';
}

void setup_terminal(void) {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    printf("\033[?25l"); // hide cursor
}

void cleanup_terminal(void) {
    printf("\033[?25h"); // show cursor
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

double now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void erase(int x, int y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        printf("\033[%d;%dH%c", y + 1, x + 1, background[y][x]);
}

void draw(int x, int y, const char *col, char sym) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        printf("\033[%d;%dH%s%c\033[0m", y + 1, x + 1, col, sym);
}

int main(void) {
    init_background();
    setup_terminal();
    atexit(cleanup_terminal);

    double start_time = now();
    int cx = WIDTH / 2, cy = HEIGHT / 2;
    double mid_a = 29.0, mid_b = 8.5;

    // Checkpoints in clockwise order: bottom → left → top → right
    double cpx[4] = { cx, cx - mid_a, cx, cx + mid_a };
    double cpy[4] = { cy + mid_b, cy, cy - mid_b, cy };

    // Player starts at bottom facing left (clockwise direction)
    player.x = cx;
    player.y = cy + mid_b - 2;
    player.angle = M_PI;
    player.speed = 0;
    player.laps = 0;
    player.cp = 0;
    player.prev_px = player.prev_py = -1;

    // Enemies on perfect elliptical path
    double phase[3] = { 0.0, 2.2, 4.4 };
    double speeds[3] = { 0.026, 0.029, 0.031 };
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].t = phase[i];
        enemies[i].speed_inc = speeds[i];
        enemies[i].laps = 0;
        enemies[i].prev_px = enemies[i].prev_py = -1;
    }

    // Initial screen
    printf("\033[2J\033[H");
    for (int i = 0; i < HEIGHT; i++) printf("%s\n", background[i]);
    printf("\033[1;15H\033[1;37m=== GROK TERMINAL RACER ===\033[0m");
    printf("\033[3;1HLaps: 0/3     Time: 0.00");
    printf("\033[5;1HW↑ S↓ A← D→ steer     Q quit");
    fflush(stdout);

    int frame = 0;
    while (1) {
        frame++;
        double elapsed = now() - start_time;

        // UI update (in place)
        printf("\033[3;6H%d/3   ", player.laps);
        printf("\033[3;22H%.2f", elapsed);
        fflush(stdout);

        // Input (non-blocking, handles key repeat naturally)
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 0};
        int key_w = 0, key_s = 0, key_a = 0, key_d = 0;
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            char buf[32];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            for (ssize_t i = 0; i < n; i++) {
                char k = buf[i];
                if (k == 'q' || k == 'Q') goto game_over;
                if (k == 'w' || k == 'W') key_w = 1;
                if (k == 's' || k == 'S') key_s = 1;
                if (k == 'a' || k == 'A') key_a = 1;
                if (k == 'd' || k == 'D') key_d = 1;
            }
        }

        // === PLAYER PHYSICS ===
        if (key_w) player.speed += 0.28;
        if (key_s) player.speed -= 0.45;
        if (key_a) player.angle += 0.09;
        if (key_d) player.angle -= 0.09;

        player.speed *= 0.982;

        double nx = player.x + cos(player.angle) * player.speed;
        double ny = player.y + sin(player.angle) * player.speed;
        int testx = (int)(nx + 0.5);
        int testy = (int)(ny + 0.5);

        if (testx < 0 || testx >= WIDTH || testy < 0 || testy >= HEIGHT || background[testy][testx] == '#')
            player.speed *= -0.35; // bounce + slow
        else {
            player.x = nx;
            player.y = ny;
        }

        // Checkpoint / lap logic
        if (hypot(player.x - cpx[player.cp], player.y - cpy[player.cp]) < 9.0) {
            player.cp = (player.cp + 1) % 4;
            if (player.cp == 0) player.laps++;
        }

        // === ENEMIES (perfect oval path) ===
        int max_enemy_laps = 0;
        for (int i = 0; i < NUM_ENEMIES; i++) {
            enemies[i].t += enemies[i].speed_inc;
            double ex = cx + mid_a * cos(enemies[i].t);
            double ey = cy + mid_b * sin(enemies[i].t);
            enemies[i].laps = (int)(enemies[i].t / (2 * M_PI));

            if (enemies[i].laps > max_enemy_laps)
                max_enemy_laps = enemies[i].laps;

            erase(enemies[i].prev_px, enemies[i].prev_py);
            int px = (int)(ex + 0.5);
            int py = (int)(ey + 0.5);
            draw(px, py, enemy_colors[i], 'O');
            enemies[i].prev_px = px;
            enemies[i].prev_py = py;
        }

        // Draw player
        erase(player.prev_px, player.prev_py);
        int ppx = (int)(player.x + 0.5);
        int ppy = (int)(player.y + 0.5);
        draw(ppx, ppy, "\033[31m", 'O'); // red player
        player.prev_px = ppx;
        player.prev_py = ppy;

        // Victory condition (first to 3 laps)
        if (player.laps >= 3 || max_enemy_laps >= 3) {
            game_over:
            cleanup_terminal();
            printf("\033[2J\033[H");
            printf("\033[8;25H");
            if (player.laps >= 3 && player.laps >= max_enemy_laps) {
                printf("\033[1;32m★★★ YOU WIN! ★★★\033[0m\n\n");
                printf("\033[11;30HTime: %.2f seconds   Laps: %d\n", elapsed, player.laps);
            } else {
                printf("\033[1;31m═══ YOU LOSE ═══\033[0m\n\n");
                printf("\033[11;30HAn enemy finished first!\n");
            }
            printf("\033[20;1H");
            fflush(stdout);
            sleep(1);
            // temporary echo for final keypress
            struct termios t = oldt;
            t.c_lflag |= ICANON | ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &t);
            getchar();
            return 0;
        }

        fflush(stdout);
        usleep(42000); // ~24 FPS, smooth and responsive
    }
}
