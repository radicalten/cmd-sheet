#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #define CLEAR "cls"
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #define CLEAR "clear"
#endif

// Game constants
#define SCREEN_WIDTH 70
#define SCREEN_HEIGHT 25
#define TARGET_LAPS 3

// Player structure
typedef struct {
    float x, y;
    float dx, dy;
    float angle;
    int laps;
    int crossed_middle;
    int crossed_finish;
} Player;

// Game state
typedef struct {
    char track[SCREEN_HEIGHT][SCREEN_WIDTH];
    Player player;
    clock_t start_time;
    int game_over;
    float final_time;
} GameState;

#ifndef _WIN32
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(0, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(0, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(0, TCSAFLUSH, &raw);
}

int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch() {
    int c = getchar();
    return c;
}

void msleep(int ms) {
    usleep(ms * 1000);
}
#else
void enable_raw_mode() {}
void disable_raw_mode() {}
void msleep(int ms) {
    Sleep(ms);
}
#endif

void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void draw_circle(char track[SCREEN_HEIGHT][SCREEN_WIDTH], int cx, int cy, int rx, int ry, char fill) {
    for (int angle = 0; angle < 360; angle += 1) {
        float rad = angle * 3.14159265f / 180.0f;
        int x = cx + (int)(rx * cos(rad));
        int y = cy + (int)(ry * sin(rad));
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            track[y][x] = fill;
        }
    }
}

void init_track(GameState* game) {
    // Fill with grass
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            game->track[y][x] = '.';
        }
    }
    
    // Draw figure-8 track (two circles connected)
    // Top loop - outer
    draw_circle(game->track, 35, 8, 16, 6, '#');
    // Top loop - inner
    draw_circle(game->track, 35, 8, 12, 4, ' ');
    
    // Bottom loop - outer
    draw_circle(game->track, 35, 17, 16, 6, '#');
    // Bottom loop - inner  
    draw_circle(game->track, 35, 17, 12, 4, ' ');
    
    // Draw finish line at top
    for (int i = -3; i <= 3; i++) {
        if (35+i >= 0 && 35+i < SCREEN_WIDTH && 2 >= 0 && 2 < SCREEN_HEIGHT) {
            game->track[2][35+i] = '=';
        }
    }
}

void init_game(GameState* game) {
    init_track(game);
    
    game->player.x = 35.0f;
    game->player.y = 4.0f;
    game->player.angle = 3.14159f / 2;  // Pointing down
    game->player.dx = 0;
    game->player.dy = 0;
    game->player.laps = 0;
    game->player.crossed_middle = 0;
    game->player.crossed_finish = 0;
    game->start_time = clock();
    game->game_over = 0;
    game->final_time = 0;
}

int is_on_track(GameState* game, int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return 0;
    }
    
    char c = game->track[y][x];
    return (c == ' ' || c == '=');
}

void update_player(GameState* game, char input) {
    float accel = 0.15f;
    float turn_speed = 0.12f;
    float friction = 0.98f;
    float max_speed = 1.5f;
    
    // Handle input
    if (input == 'w' || input == 'W') {
        game->player.dx += cos(game->player.angle) * accel;
        game->player.dy += sin(game->player.angle) * accel;
    }
    if (input == 's' || input == 'S') {
        game->player.dx -= cos(game->player.angle) * accel * 0.5f;
        game->player.dy -= sin(game->player.angle) * accel * 0.5f;
    }
    if (input == 'a' || input == 'A') {
        game->player.angle -= turn_speed;
    }
    if (input == 'd' || input == 'D') {
        game->player.angle += turn_speed;
    }
    
    // Apply friction
    game->player.dx *= friction;
    game->player.dy *= friction;
    
    // Limit speed
    float speed = sqrt(game->player.dx * game->player.dx + game->player.dy * game->player.dy);
    if (speed > max_speed) {
        game->player.dx = (game->player.dx / speed) * max_speed;
        game->player.dy = (game->player.dy / speed) * max_speed;
    }
    
    // Update position
    float new_x = game->player.x + game->player.dx;
    float new_y = game->player.y + game->player.dy;
    
    // Check collision
    if (is_on_track(game, (int)new_x, (int)new_y)) {
        game->player.x = new_x;
        game->player.y = new_y;
    } else {
        // Collision - bounce back
        game->player.dx *= -0.3f;
        game->player.dy *= -0.3f;
    }
    
    // Lap detection
    int py = (int)game->player.y;
    int px = (int)game->player.x;
    
    // Check if crossed middle of track (around y=12-13)
    if (py >= 11 && py <= 14 && px >= 30 && px <= 40) {
        if (!game->player.crossed_middle) {
            game->player.crossed_middle = 1;
        }
    }
    
    // Check if crossed finish line
    if (py >= 2 && py <= 5 && px >= 32 && px <= 38) {
        if (game->player.crossed_middle && !game->player.crossed_finish) {
            game->player.crossed_finish = 1;
            game->player.laps++;
            game->player.crossed_middle = 0;
            
            if (game->player.laps >= TARGET_LAPS) {
                game->game_over = 1;
                game->final_time = (float)(clock() - game->start_time) / CLOCKS_PER_SEC;
            }
        }
    } else {
        game->player.crossed_finish = 0;
    }
}

void render(GameState* game) {
    clear_screen();
    
    // Create display buffer
    char display[SCREEN_HEIGHT][SCREEN_WIDTH];
    memcpy(display, game->track, sizeof(display));
    
    // Draw player
    int px = (int)game->player.x;
    int py = (int)game->player.y;
    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        display[py][px] = 'X';
    }
    
    // Render display
    printf("╔");
    for (int i = 0; i < SCREEN_WIDTH; i++) printf("═");
    printf("╗\n");
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        printf("║");
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            char c = display[y][x];
            if (c == '#') {
                printf("█");
            } else if (c == '=') {
                printf("▓");
            } else if (c == 'X') {
                printf("▲");
            } else if (c == ' ') {
                printf(" ");
            } else {
                printf("░");
            }
        }
        printf("║\n");
    }
    
    printf("╚");
    for (int i = 0; i < SCREEN_WIDTH; i++) printf("═");
    printf("╝\n");
    
    // Draw HUD
    float elapsed = (float)(clock() - game->start_time) / CLOCKS_PER_SEC;
    float speed = sqrt(game->player.dx * game->player.dx + game->player.dy * game->player.dy);
    
    printf("\n  LAP: %d/%d  |  TIME: %.2f s  |  SPEED: %.1f\n", 
           game->player.laps, TARGET_LAPS, elapsed, speed * 20);
    printf("  Controls: [W] Accelerate  [S] Brake  [A] Left  [D] Right  [Q] Quit\n");
}

void show_victory(GameState* game) {
    clear_screen();
    printf("\n\n\n");
    printf("         ╔═══════════════════════════════════════════╗\n");
    printf("         ║                                           ║\n");
    printf("         ║            🏁 RACE COMPLETE! 🏁           ║\n");
    printf("         ║                                           ║\n");
    printf("         ║         You finished %d laps!             ║\n", TARGET_LAPS);
    printf("         ║                                           ║\n");
    printf("         ║       Final Time: %.2f seconds           ║\n", game->final_time);
    printf("         ║                                           ║\n");
    printf("         ║         🏆 CONGRATULATIONS! 🏆            ║\n");
    printf("         ║                                           ║\n");
    printf("         ╚═══════════════════════════════════════════╝\n");
    printf("\n\n         Press any key to exit...\n");
    
    #ifdef _WIN32
    _getch();
    #else
    getchar();
    #endif
}

int main() {
    GameState game;
    
    enable_raw_mode();
    
    clear_screen();
    printf("\n\n");
    printf("         ╔═══════════════════════════════════════════╗\n");
    printf("         ║                                           ║\n");
    printf("         ║          TOP-DOWN RACING GAME             ║\n");
    printf("         ║                                           ║\n");
    printf("         ║      Complete %d laps as fast as you can! ║\n", TARGET_LAPS);
    printf("         ║                                           ║\n");
    printf("         ║         Press any key to start...         ║\n");
    printf("         ║                                           ║\n");
    printf("         ╚═══════════════════════════════════════════╝\n");
    
    #ifdef _WIN32
    _getch();
    #else
    getchar();
    #endif
    
    init_game(&game);
    
    // Game loop
    while (!game.game_over) {
        char input = 0;
        
        // Get input
        if (kbhit()) {
            input = getch();
            if (input == 'q' || input == 'Q' || input == 27) {  // 27 = ESC
                break;
            }
        }
        
        // Update
        update_player(&game, input);
        
        // Render
        render(&game);
        
        // Frame delay (targeting ~30 FPS)
        msleep(33);
    }
    
    if (game.game_over) {
        show_victory(&game);
    }
    
    disable_raw_mode();
    clear_screen();
    
    return 0;
}
