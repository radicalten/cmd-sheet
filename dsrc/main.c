#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>

#define WIDTH 70
#define HEIGHT 25
#define MAX_ENEMIES 3
#define TOTAL_LAPS 3
#define FPS 20

// ANSI escape codes
#define CLEAR_SCREEN "\033[2J"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define MOVE_CURSOR(x, y) printf("\033[%d;%dH", (y), (x))

typedef struct {
    float x, y;
    float vx, vy;
    int laps;
    int checkpoint;
} Car;

char track[HEIGHT][WIDTH];
Car player;
Car enemies[MAX_ENEMIES];
int game_running = 1;
time_t start_time;
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf(SHOW_CURSOR);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf(HIDE_CURSOR);
}

int kbhit() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    int ch = getchar();
    fcntl(STDIN_FILENO, F_SETFL, flags);
    return ch;
}

void init_track() {
    // Fill with grass
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            track[y][x] = '.';
    
    // Draw oval track
    for (int y = 3; y < HEIGHT - 3; y++) {
        for (int x = 5; x < WIDTH - 5; x++) {
            int dx = (x < WIDTH/2) ? x - 10 : x - (WIDTH - 10);
            int dy = y - HEIGHT/2;
            int outer = dx*dx/400 + dy*dy/80;
            int inner = dx*dx/200 + dy*dy/40;
            
            if (outer <= 1 && inner > 1) {
                track[y][x] = ' ';
            } else if (outer <= 1 && inner <= 1) {
                track[y][x] = '#';
            }
        }
    }
    
    // Draw outer walls
    for (int y = 3; y < HEIGHT - 3; y++) {
        for (int x = 5; x < WIDTH - 5; x++) {
            int dx = (x < WIDTH/2) ? x - 10 : x - (WIDTH - 10);
            int dy = y - HEIGHT/2;
            int outer = dx*dx/400 + dy*dy/80;
            if (outer >= 1 && outer <= 1.15) track[y][x] = '#';
        }
    }
    
    // Finish line
    for (int i = 0; i < 4; i++)
        track[HEIGHT/2 - 2 + i][12] = '=';
}

int is_on_track(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
    return (track[y][x] == ' ' || track[y][x] == '=');
}

void init_game() {
    init_track();
    player.x = 13; player.y = HEIGHT/2;
    player.vx = 0; player.vy = 0;
    player.laps = 0; player.checkpoint = 0;
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].x = 13;
        enemies[i].y = HEIGHT/2 - 1 - i;
        enemies[i].vx = 0.4;
        enemies[i].vy = 0;
        enemies[i].laps = 0;
        enemies[i].checkpoint = 0;
    }
    start_time = time(NULL);
}

void update_enemy(Car *e) {
    float tx, ty;
    
    // Waypoints around track
    if (e->checkpoint == 0) { tx = WIDTH - 15; ty = HEIGHT/2; }
    else if (e->checkpoint == 1) { tx = WIDTH - 15; ty = HEIGHT - 7; }
    else if (e->checkpoint == 2) { tx = 15; ty = HEIGHT - 7; }
    else if (e->checkpoint == 3) { tx = 15; ty = 6; }
    else { tx = 13; ty = HEIGHT/2; }
    
    float dx = tx - e->x;
    float dy = ty - e->y;
    float dist = sqrt(dx*dx + dy*dy);
    
    if (dist < 4) {
        e->checkpoint = (e->checkpoint + 1) % 5;
        if (e->checkpoint == 0) e->laps++;
    }
    
    if (dist > 0) {
        e->vx = (dx / dist) * 0.5;
        e->vy = (dy / dist) * 0.5;
    }
    
    float nx = e->x + e->vx;
    float ny = e->y + e->vy;
    if (is_on_track((int)nx, (int)ny)) {
        e->x = nx; e->y = ny;
    }
}

void handle_input() {
    int ch = kbhit();
    if (ch == 'w' || ch == 'W') player.vy -= 0.3;
    if (ch == 's' || ch == 'S') player.vy += 0.3;
    if (ch == 'a' || ch == 'A') player.vx -= 0.3;
    if (ch == 'd' || ch == 'D') player.vx += 0.3;
    if (ch == 'q' || ch == 'Q') game_running = 0;
    
    player.vx *= 0.85;
    player.vy *= 0.85;
    
    float speed = sqrt(player.vx*player.vx + player.vy*player.vy);
    if (speed > 1.2) {
        player.vx = (player.vx / speed) * 1.2;
        player.vy = (player.vy / speed) * 1.2;
    }
}

void update_player() {
    float nx = player.x + player.vx;
    float ny = player.y + player.vy;
    
    if (is_on_track((int)nx, (int)ny)) {
        player.x = nx; player.y = ny;
        
        // Checkpoint detection
        if (player.checkpoint == 0 && player.x > WIDTH/2 && player.y < HEIGHT/2) {
            player.checkpoint = 1;
        } else if (player.checkpoint == 1 && player.x > WIDTH/2 && player.y > HEIGHT/2) {
            player.checkpoint = 2;
        } else if (player.checkpoint == 2 && player.x < WIDTH/2 && player.y > HEIGHT/2) {
            player.checkpoint = 3;
        } else if (player.checkpoint == 3 && player.x < WIDTH/2 && player.y < HEIGHT/2) {
            player.checkpoint = 0;
            player.laps++;
        }
    } else {
        player.vx *= -0.3;
        player.vy *= -0.3;
    }
}

void draw_initial() {
    printf(CLEAR_SCREEN);
    MOVE_CURSOR(1, 1);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            putchar(track[y][x]);
        }
        putchar('\n');
    }
    printf("\nControls: WASD to move | Q to quit\n");
    printf("Status: ");
    fflush(stdout);
}

void update_screen() {
    static int old_px = -1, old_py = -1;
    static int old_ex[MAX_ENEMIES], old_ey[MAX_ENEMIES];
    
    // Clear old positions
    if (old_px >= 0 && old_py >= 0) {
        MOVE_CURSOR(old_px + 1, old_py + 1);
        putchar(track[old_py][old_px]);
    }
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (old_ex[i] >= 0 && old_ey[i] >= 0) {
            MOVE_CURSOR(old_ex[i] + 1, old_ey[i] + 1);
            putchar(track[old_ey[i]][old_ex[i]]);
        }
    }
    
    // Draw enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        old_ex[i] = (int)enemies[i].x;
        old_ey[i] = (int)enemies[i].y;
        MOVE_CURSOR(old_ex[i] + 1, old_ey[i] + 1);
        printf("\033[31mE\033[0m"); // Red enemy
    }
    
    // Draw player
    old_px = (int)player.x;
    old_py = (int)player.y;
    MOVE_CURSOR(old_px + 1, old_py + 1);
    printf("\033[32mP\033[0m"); // Green player
    
    // Update status
    int elapsed = (int)(time(NULL) - start_time);
    MOVE_CURSOR(9, HEIGHT + 3);
    printf("Lap %d/%d | Time %02d:%02d | Speed %.0f%%   ",
           player.laps, TOTAL_LAPS, elapsed/60, elapsed%60,
           sqrt(player.vx*player.vx + player.vy*player.vy) / 1.2 * 100);
    fflush(stdout);
}

void show_victory() {
    int total = (int)(time(NULL) - start_time);
    printf(CLEAR_SCREEN);
    MOVE_CURSOR(1, 5);
    printf("\n");
    printf("    ╔══════════════════════════════════╗\n");
    printf("    ║                                  ║\n");
    printf("    ║      🏁 RACE COMPLETE! 🏁        ║\n");
    printf("    ║                                  ║\n");
    printf("    ║     Total Time: %02d:%02d          ║\n", total/60, total%60);
    printf("    ║                                  ║\n");
    printf("    ║       Great driving!             ║\n");
    printf("    ║                                  ║\n");
    printf("    ╚══════════════════════════════════╝\n");
    printf("\n\n");
    fflush(stdout);
    sleep(4);
}

int main() {
    enable_raw_mode();
    init_game();
    draw_initial();
    
    struct timespec sleep_time = {0, 1000000000 / FPS};
    
    while (game_running && player.laps < TOTAL_LAPS) {
        handle_input();
        update_player();
        for (int i = 0; i < MAX_ENEMIES; i++)
            update_enemy(&enemies[i]);
        update_screen();
        nanosleep(&sleep_time, NULL);
    }
    
    if (player.laps >= TOTAL_LAPS)
        show_victory();
    
    disable_raw_mode();
    printf(CLEAR_SCREEN);
    MOVE_CURSOR(1, 1);
    printf("Thanks for playing!\n");
    return 0;
}
