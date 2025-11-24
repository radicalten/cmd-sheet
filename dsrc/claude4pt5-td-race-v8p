#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>

// Game constants
#define WIDTH 70
#define HEIGHT 24
#define MAX_ENEMIES 3
#define LAPS_TO_WIN 3

// ANSI codes
#define CLEAR "\033[2J\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define RED "\033[91m"
#define GREEN "\033[92m"
#define YELLOW "\033[93m"
#define BLUE "\033[94m"
#define MAGENTA "\033[95m"
#define CYAN "\033[96m"
#define WHITE "\033[97m"

typedef struct {
    float x, y;
    float dx, dy;
    float angle;
    int lap;
    int checkpoint;
} Car;

char track[HEIGHT][WIDTH];
Car player;
Car enemies[MAX_ENEMIES];
time_t start_time;
struct termios orig_termios;

void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf(SHOW_CURSOR CLEAR);
}

void setup_terminal() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
}

void init_track() {
    memset(track, ' ', sizeof(track));
    
    // Outer boundary
    for(int j = 5; j < 65; j++) {
        track[1][j] = '#';
        track[22][j] = '#';
    }
    for(int i = 1; i < 23; i++) {
        track[i][5] = '#';
        track[i][64] = '#';
    }
    
    // Inner island
    for(int j = 15; j < 55; j++) {
        track[6][j] = '#';
        track[17][j] = '#';
    }
    for(int i = 6; i < 18; i++) {
        track[i][15] = '#';
        track[i][54] = '#';
    }
    
    // Start/finish line
    for(int i = 2; i < 6; i++) {
        track[i][35] = '=';
    }
}

int is_valid_pos(float x, float y) {
    int ix = (int)x, iy = (int)y;
    if(ix < 0 || ix >= WIDTH || iy < 0 || iy >= HEIGHT) return 0;
    char c = track[iy][ix];
    return c != '#';
}

void update_checkpoint(Car *car) {
    int ix = (int)car->x, iy = (int)car->y;
    
    if(car->checkpoint == 0 && ix > 50 && iy > 8 && iy < 15) {
        car->checkpoint = 1;
    } else if(car->checkpoint == 1 && ix > 25 && ix < 45 && iy > 19) {
        car->checkpoint = 2;
    } else if(car->checkpoint == 2 && ix < 20 && iy > 8 && iy < 15) {
        car->checkpoint = 3;
    } else if(car->checkpoint == 3 && ix > 33 && ix < 37 && iy < 6) {
        car->lap++;
        car->checkpoint = 0;
    }
}

void update_player(char input) {
    float speed = 0.6f;
    float turn = 0.15f;
    
    if(input == 'w' || input == 'W') {
        float new_x = player.x + cos(player.angle) * speed;
        float new_y = player.y + sin(player.angle) * speed;
        if(is_valid_pos(new_x, new_y)) {
            player.x = new_x;
            player.y = new_y;
        }
    }
    if(input == 's' || input == 'S') {
        float new_x = player.x - cos(player.angle) * speed * 0.5f;
        float new_y = player.y - sin(player.angle) * speed * 0.5f;
        if(is_valid_pos(new_x, new_y)) {
            player.x = new_x;
            player.y = new_y;
        }
    }
    if(input == 'a' || input == 'A') player.angle -= turn;
    if(input == 'd' || input == 'D') player.angle += turn;
    
    update_checkpoint(&player);
}

void update_enemy(Car *enemy) {
    float target_x, target_y;
    int ix = (int)enemy->x, iy = (int)enemy->y;
    
    // Simple waypoint navigation
    if(iy < 8) {
        target_x = 58.0f; target_y = 11.0f;
    } else if(ix > 48) {
        target_x = 35.0f; target_y = 20.0f;
    } else if(iy > 16) {
        target_x = 12.0f; target_y = 11.0f;
    } else {
        target_x = 35.0f; target_y = 3.0f;
    }
    
    float dx = target_x - enemy->x;
    float dy = target_y - enemy->y;
    float dist = sqrt(dx*dx + dy*dy);
    
    if(dist > 0.5f) {
        float new_x = enemy->x + (dx/dist) * 0.4f;
        float new_y = enemy->y + (dy/dist) * 0.4f;
        if(is_valid_pos(new_x, new_y)) {
            enemy->x = new_x;
            enemy->y = new_y;
        }
    }
    
    update_checkpoint(enemy);
}

void render() {
    printf("\033[H"); // Move cursor to home
    
    char display[HEIGHT][WIDTH];
    memcpy(display, track, sizeof(track));
    
    // Draw enemies
    for(int i = 0; i < MAX_ENEMIES; i++) {
        int ex = (int)enemies[i].x, ey = (int)enemies[i].y;
        if(ex >= 0 && ex < WIDTH && ey >= 0 && ey < HEIGHT)
            display[ey][ex] = 'E';
    }
    
    // Draw player
    int px = (int)player.x, py = (int)player.y;
    if(px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
        display[py][px] = 'P';
    
    // Render with colors
    for(int i = 0; i < HEIGHT; i++) {
        for(int j = 0; j < WIDTH; j++) {
            char c = display[i][j];
            switch(c) {
                case '#': printf(RED "█" RESET); break;
                case '=': printf(BOLD YELLOW "=" RESET); break;
                case 'P': printf(BOLD CYAN "P" RESET); break;
                case 'E': printf(BOLD MAGENTA "E" RESET); break;
                default:  printf(GREEN "·" RESET); break;
            }
        }
        printf("\n");
    }
    
    // HUD
    time_t elapsed = time(NULL) - start_time;
    printf(BOLD "\n🏁 Lap: %d/%d | ⏱️  Time: %02ld:%02ld | 🎮 WASD=Drive Q=Quit" RESET,
           player.lap, LAPS_TO_WIN, elapsed/60, elapsed%60);
    fflush(stdout);
}

void show_victory() {
    time_t total = time(NULL) - start_time;
    printf(CLEAR BOLD YELLOW);
    printf("\n\n\n");
    printf("        ╔═══════════════════════════════════╗\n");
    printf("        ║                                   ║\n");
    printf("        ║       🏆 VICTORY! 🏆              ║\n");
    printf("        ║                                   ║\n");
    printf("        ║    You completed %d laps!         ║\n", LAPS_TO_WIN);
    printf("        ║    Final time: %02ld:%02ld             ║\n", total/60, total%60);
    printf("        ║                                   ║\n");
    printf("        ║   Press any key to exit...        ║\n");
    printf("        ║                                   ║\n");
    printf("        ╚═══════════════════════════════════╝\n");
    printf(RESET);
    
    getchar();
}

int main() {
    printf(CLEAR HIDE_CURSOR);
    setup_terminal();
    
    init_track();
    
    // Initialize player
    player = (Car){35.0f, 3.5f, 0, 0, M_PI/2, 0, 0};
    
    // Initialize enemies
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i] = (Car){33.0f + i*1.5f, 4.0f + i*0.5f, 0, 0, 0, 0, 0};
    }
    
    start_time = time(NULL);
    
    // Game loop
    while(player.lap < LAPS_TO_WIN) {
        char input = 0;
        int ch = getchar();
        if(ch != EOF) {
            input = ch;
            if(input == 'q' || input == 'Q') break;
        }
        
        update_player(input);
        for(int i = 0; i < MAX_ENEMIES; i++)
            update_enemy(&enemies[i]);
        
        render();
        usleep(50000); // ~20 FPS
    }
    
    if(player.lap >= LAPS_TO_WIN)
        show_victory();
    
    return 0;
}
