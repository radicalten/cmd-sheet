#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>
#include <math.h>

#define WIDTH 80
#define HEIGHT 24
#define TOTAL_LAPS 3

typedef struct {
    double x, y;
    double speed;
    double angle;
    int laps;
    int passed_checkpoint;
} Player;

char track[HEIGHT][WIDTH];
Player player;
time_t start_time;
int game_over = 0;
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l");
}

int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

void draw_circle(int cx, int cy, int r_inner, int r_outer) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int dx = x - cx;
            int dy = (y - cy) * 2;
            double dist = sqrt(dx*dx + dy*dy);
            
            if (dist >= r_inner && dist <= r_outer) {
                if (track[y][x] == '.') track[y][x] = ' ';
            }
            if ((dist >= r_outer && dist <= r_outer + 1) ||
                (dist >= r_inner - 1 && dist <= r_inner)) {
                if (track[y][x] != '|') track[y][x] = '#';
            }
        }
    }
}

void init_track() {
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            track[y][x] = '.';
    
    draw_circle(25, 12, 7, 14);
    draw_circle(55, 12, 7, 14);
    
    for (int y = 8; y <= 16; y++) {
        for (int x = 35; x <= 45; x++) {
            if (track[y][x] == '.') track[y][x] = ' ';
        }
    }
    
    for (int y = 10; y <= 14; y++) {
        track[y][40] = '|';
        track[y][30] = 'X';
    }
}

void init_player() {
    player.x = 42;
    player.y = 12;
    player.speed = 0;
    player.angle = 0;
    player.laps = 0;
    player.passed_checkpoint = 0;
}

int is_valid_pos(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
    char c = track[y][x];
    return (c == ' ' || c == '|' || c == 'X');
}

void update_player(char input) {
    if (input == 'w') player.speed = (player.speed < 1.5) ? player.speed + 0.15 : 1.5;
    if (input == 's') player.speed = (player.speed > -0.5) ? player.speed - 0.15 : -0.5;
    if (input == 'a') player.angle -= 0.2;
    if (input == 'd') player.angle += 0.2;
    
    player.speed *= 0.96;
    
    double new_x = player.x + cos(player.angle) * player.speed;
    double new_y = player.y + sin(player.angle) * player.speed * 0.5;
    
    if (is_valid_pos((int)new_x, (int)new_y)) {
        int old_x = (int)player.x;
        player.x = new_x;
        player.y = new_y;
        
        char current = track[(int)player.y][(int)player.x];
        
        if (current == 'X' && old_x > 30) {
            player.passed_checkpoint = 1;
        }
        
        if (current == '|' && old_x < 40 && player.passed_checkpoint) {
            player.laps++;
            player.passed_checkpoint = 0;
            if (player.laps >= TOTAL_LAPS) game_over = 1;
        }
    } else {
        player.speed *= 0.3;
    }
}

void draw_screen() {
    printf("\033[H");
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if ((int)player.x == x && (int)player.y == y) {
                printf("\033[1;33mO\033[0m");
            } else if (track[y][x] == '#') {
                printf("\033[32m#\033[0m");
            } else if (track[y][x] == '|') {
                printf("\033[1;31m|\033[0m");
            } else if (track[y][x] == 'X') {
                printf("\033[36mX\033[0m");
            } else {
                printf("%c", track[y][x]);
            }
        }
        printf("\n");
    }
    
    int elapsed = (int)(time(NULL) - start_time);
    printf("\n\033[1mLap: %d/%d | Time: %02d:%02d | Speed: %.1f | WASD:Move Q:Quit\033[0m",
           player.laps, TOTAL_LAPS, elapsed/60, elapsed%60, player.speed);
    fflush(stdout);
}

void victory_screen() {
    printf("\033[2J\033[H\n\n");
    int elapsed = (int)(time(NULL) - start_time);
    printf("          ╔════════════════════════════════════╗\n");
    printf("          ║    \033[1;32m🏁  RACE COMPLETE!  🏁\033[0m    ║\n");
    printf("          ╔════════════════════════════════════╝\n");
    printf("          ║                                      \n");
    printf("          ║  Final Time: \033[1;33m%02d:%02d\033[0m              \n", elapsed/60, elapsed%60);
    printf("          ║  Total Laps: \033[1;36m%d\033[0m                   \n", TOTAL_LAPS);
    printf("          ║                                      \n");
    printf("          ║  \033[1mExcellent driving!\033[0m               \n");
    printf("          ║                                      \n");
    printf("\n          Press any key to exit...\n");
    fflush(stdout);
    getchar();
}

int main() {
    enable_raw_mode();
    init_track();
    init_player();
    start_time = time(NULL);
    
    printf("\033[2J");
    draw_screen();
    
    while (!game_over) {
        char input = 0;
        if (kbhit()) {
            input = getchar();
            if (input == 'q') break;
        }
        
        update_player(input);
        draw_screen();
        usleep(50000);
    }
    
    if (game_over) victory_screen();
    
    disable_raw_mode();
    printf("Thanks for playing!\n");
    return 0;
}
