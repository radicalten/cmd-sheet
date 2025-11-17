#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// Screen dimensions
#define WIDTH 60
#define HEIGHT 30

// Game constants
#define MAX_BULLETS 50
#define MAX_ENEMIES 40
#define MAX_ENEMY_BULLETS 30
#define PLAYER_SPEED 1
#define BULLET_SPEED 1
#define ENEMY_COLS 10
#define ENEMY_ROWS 4

// Entity structure
typedef struct {
    float x, y;
    int active;
    float vx, vy;
} Entity;

// Game state
typedef struct {
    Entity player;
    Entity bullets[MAX_BULLETS];
    Entity enemies[MAX_ENEMIES];
    Entity enemy_bullets[MAX_ENEMY_BULLETS];
    int score;
    int level;
    int game_over;
    int enemies_alive;
    float enemy_formation_x;
    int enemy_direction;
    int frame_count;
} GameState;

// Terminal handling
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int kbhit() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    int ch = getchar();
    fcntl(STDIN_FILENO, F_SETFL, flags);
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

// Screen functions
void clear_screen() {
    printf("\033[2J\033[H");
}

void hide_cursor() {
    printf("\033[?25l");
}

void show_cursor() {
    printf("\033[?25h");
}

void set_color(int color) {
    printf("\033[%dm", color);
}

void reset_color() {
    printf("\033[0m");
}

// Initialize game
void init_game(GameState *game) {
    game->player.x = WIDTH / 2;
    game->player.y = HEIGHT - 3;
    game->player.active = 1;
    
    memset(game->bullets, 0, sizeof(game->bullets));
    memset(game->enemy_bullets, 0, sizeof(game->enemy_bullets));
    
    game->enemies_alive = 0;
    game->enemy_formation_x = 0;
    game->enemy_direction = 1;
    game->frame_count = 0;
    
    // Create enemy formation
    for (int row = 0; row < ENEMY_ROWS; row++) {
        for (int col = 0; col < ENEMY_COLS; col++) {
            int idx = row * ENEMY_COLS + col;
            game->enemies[idx].x = col * 5 + 5;
            game->enemies[idx].y = row * 3 + 3;
            game->enemies[idx].active = 1;
            game->enemies[idx].vx = 0;
            game->enemies[idx].vy = 0;
            game->enemies_alive++;
        }
    }
}

void start_game(GameState *game) {
    game->score = 0;
    game->level = 1;
    game->game_over = 0;
    init_game(game);
}

// Spawn player bullet
void shoot_bullet(GameState *game) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!game->bullets[i].active) {
            game->bullets[i].x = game->player.x;
            game->bullets[i].y = game->player.y - 1;
            game->bullets[i].active = 1;
            game->bullets[i].vy = -BULLET_SPEED;
            break;
        }
    }
}

// Enemy shoots
void enemy_shoot(GameState *game, int enemy_idx) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!game->enemy_bullets[i].active) {
            game->enemy_bullets[i].x = game->enemies[enemy_idx].x;
            game->enemy_bullets[i].y = game->enemies[enemy_idx].y + 1;
            game->enemy_bullets[i].active = 1;
            game->enemy_bullets[i].vx = 0;
            game->enemy_bullets[i].vy = 0.5;
            break;
        }
    }
}

// Check collision
int check_collision(float x1, float y1, float x2, float y2, float distance) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return (dx * dx + dy * dy) < (distance * distance);
}

// Update game logic
void update_game(GameState *game, char input) {
    if (game->game_over) return;
    
    game->frame_count++;
    
    // Player movement
    if (input == 'a' || input == 'A') {
        game->player.x -= PLAYER_SPEED;
        if (game->player.x < 1) game->player.x = 1;
    }
    if (input == 'd' || input == 'D') {
        game->player.x += PLAYER_SPEED;
        if (game->player.x >= WIDTH - 1) game->player.x = WIDTH - 1;
    }
    if (input == ' ') {
        shoot_bullet(game);
    }
    
    // Update player bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (game->bullets[i].active) {
            game->bullets[i].y += game->bullets[i].vy;
            if (game->bullets[i].y < 0) {
                game->bullets[i].active = 0;
            }
        }
    }
    
    // Update enemy bullets
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (game->enemy_bullets[i].active) {
            game->enemy_bullets[i].y += game->enemy_bullets[i].vy;
            game->enemy_bullets[i].x += game->enemy_bullets[i].vx;
            
            if (game->enemy_bullets[i].y >= HEIGHT) {
                game->enemy_bullets[i].active = 0;
            }
            
            // Check collision with player
            if (check_collision(game->enemy_bullets[i].x, game->enemy_bullets[i].y,
                              game->player.x, game->player.y, 1.5)) {
                game->game_over = 1;
            }
        }
    }
    
    // Enemy formation movement
    if (game->frame_count % 30 == 0) {
        game->enemy_formation_x += game->enemy_direction * 2;
        if (game->enemy_formation_x > 10 || game->enemy_formation_x < -10) {
            game->enemy_direction *= -1;
        }
    }
    
    // Update enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) continue;
        
        // Formation position
        int row = i / ENEMY_COLS;
        int col = i % ENEMY_COLS;
        float home_x = col * 5 + 5 + game->enemy_formation_x;
        float home_y = row * 3 + 3;
        
        // Dive bomb behavior
        if (game->enemies[i].vy == 0 && rand() % 500 == 0) {
            game->enemies[i].vy = 0.3;
            game->enemies[i].vx = (rand() % 2 == 0) ? 0.2 : -0.2;
        }
        
        // If diving
        if (game->enemies[i].vy > 0) {
            game->enemies[i].x += game->enemies[i].vx;
            game->enemies[i].y += game->enemies[i].vy;
            
            // Return to formation
            if (game->enemies[i].y > HEIGHT - 5) {
                game->enemies[i].vx = 0;
                game->enemies[i].vy = -0.3;
            }
            
            // Check if back in formation
            if (game->enemies[i].y < home_y && game->enemies[i].vy < 0) {
                game->enemies[i].x = home_x;
                game->enemies[i].y = home_y;
                game->enemies[i].vx = 0;
                game->enemies[i].vy = 0;
            }
            
            // Check collision with player
            if (check_collision(game->enemies[i].x, game->enemies[i].y,
                              game->player.x, game->player.y, 1.5)) {
                game->game_over = 1;
            }
        } else {
            game->enemies[i].x = home_x;
            game->enemies[i].y = home_y;
        }
        
        // Random shooting
        if (rand() % 200 == 0) {
            enemy_shoot(game, i);
        }
    }
    
    // Check bullet collisions with enemies
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!game->bullets[i].active) continue;
        
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (!game->enemies[j].active) continue;
            
            if (check_collision(game->bullets[i].x, game->bullets[i].y,
                              game->enemies[j].x, game->enemies[j].y, 1.5)) {
                game->bullets[i].active = 0;
                game->enemies[j].active = 0;
                game->score += 100;
                game->enemies_alive--;
                break;
            }
        }
    }
    
    // Check for level complete
    if (game->enemies_alive == 0) {
        game->level++;
        init_game(game);
    }
}

// Render game
void render_game(GameState *game) {
    char buffer[HEIGHT][WIDTH + 1];
    
    // Clear buffer
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            buffer[y][x] = ' ';
        }
        buffer[y][WIDTH] = '\0';
    }
    
    // Draw borders
    for (int x = 0; x < WIDTH; x++) {
        buffer[0][x] = '=';
        buffer[HEIGHT - 1][x] = '=';
    }
    for (int y = 0; y < HEIGHT; y++) {
        buffer[y][0] = '|';
        buffer[y][WIDTH - 1] = '|';
    }
    
    // Draw player
    if (game->player.active) {
        int px = (int)game->player.x;
        int py = (int)game->player.y;
        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            buffer[py][px] = 'A';
        }
    }
    
    // Draw enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game->enemies[i].active) {
            int ex = (int)game->enemies[i].x;
            int ey = (int)game->enemies[i].y;
            if (ex >= 0 && ex < WIDTH && ey >= 0 && ey < HEIGHT) {
                buffer[ey][ex] = 'W';
            }
        }
    }
    
    // Draw bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (game->bullets[i].active) {
            int bx = (int)game->bullets[i].x;
            int by = (int)game->bullets[i].y;
            if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT) {
                buffer[by][bx] = '|';
            }
        }
    }
    
    // Draw enemy bullets
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (game->enemy_bullets[i].active) {
            int bx = (int)game->enemy_bullets[i].x;
            int by = (int)game->enemy_bullets[i].y;
            if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT) {
                buffer[by][bx] = '*';
            }
        }
    }
    
    // Print buffer
    clear_screen();
    
    set_color(36); // Cyan
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", buffer[y]);
    }
    reset_color();
    
    // Print UI
    printf("\n");
    set_color(33); // Yellow
    printf("SCORE: %d  LEVEL: %d  ENEMIES: %d\n", 
           game->score, game->level, game->enemies_alive);
    reset_color();
    
    if (game->game_over) {
        set_color(31); // Red
        printf("\n*** GAME OVER! Final Score: %d ***\n", game->score);
        printf("Press 'R' to restart or 'Q' to quit\n");
        reset_color();
    } else {
        printf("Controls: A/D - Move, SPACE - Shoot, Q - Quit\n");
    }
    
    fflush(stdout);
}

// Main game loop
int main() {
    srand(time(NULL));
    enable_raw_mode();
    hide_cursor();
    
    GameState game;
    start_game(&game);
    
    char input = 0;
    int running = 1;
    
    while (running) {
        // Get input
        input = 0;
        if (kbhit()) {
            input = getchar();
            if (input == 'q' || input == 'Q') {
                running = 0;
            }
            if (game.game_over && (input == 'r' || input == 'R')) {
                start_game(&game);
            }
        }
        
        // Update and render
        update_game(&game, input);
        render_game(&game);
        
        // Frame delay (~60 FPS)
        usleep(16666);
    }
    
    show_cursor();
    clear_screen();
    printf("Thanks for playing!\n");
    
    return 0;
}
