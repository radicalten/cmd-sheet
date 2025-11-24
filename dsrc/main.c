#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// Screen dimensions
#define SCREEN_WIDTH 78
#define SCREEN_HEIGHT 22

// Game constants
#define MAX_LAPS 3
#define NUM_ENEMIES 3
#define FRAME_DELAY 50000 // microseconds (50ms = ~20 FPS)

// Track elements
#define TRACK_WALL '#'
#define TRACK_ROAD ' '
#define TRACK_FINISH '|'

// Car structure
typedef struct {
    float x, y;
    float vx, vy;
    float angle;
    int lap;
    int finished;
    char symbol;
    int checkpoint;
    float ai_offset;
} Car;

// Globals
char track[SCREEN_HEIGHT][SCREEN_WIDTH + 1];
Car player;
Car enemies[NUM_ENEMIES];
time_t start_time;
int game_over = 0;
struct termios orig_termios;

// Terminal handling functions
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
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void hide_cursor() {
    printf("\033[?25l");
}

void show_cursor() {
    printf("\033[?25h");
}

// Initialize the track (oval circuit)
void init_track() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int outer_wall = (y <= 1 || y >= SCREEN_HEIGHT - 2 || 
                            x <= 2 || x >= SCREEN_WIDTH - 3);
            
            // Inner wall creates oval shape
            int dx = x - SCREEN_WIDTH / 2;
            int dy = (y - SCREEN_HEIGHT / 2) * 2;
            int inner_wall = ((dx * dx) / 400 + (dy * dy) / 100 < 1) &&
                           ((dx * dx) / 300 + (dy * dy) / 64 < 1);
            
            if (outer_wall || inner_wall) {
                track[y][x] = TRACK_WALL;
            } else {
                track[y][x] = TRACK_ROAD;
            }
        }
        track[y][SCREEN_WIDTH] = '\0';
    }
    
    // Start/finish line
    for (int y = SCREEN_HEIGHT / 2 - 3; y <= SCREEN_HEIGHT / 2 + 3; y++) {
        if (track[y][8] == TRACK_ROAD) {
            track[y][8] = TRACK_FINISH;
        }
    }
}

// Initialize cars
void init_cars() {
    player.x = 10;
    player.y = SCREEN_HEIGHT / 2;
    player.vx = 0;
    player.vy = 0;
    player.angle = 0;
    player.lap = 0;
    player.finished = 0;
    player.symbol = 'P';
    player.checkpoint = 0;
    
    char symbols[] = {'A', 'B', 'C'};
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].x = 10;
        enemies[i].y = SCREEN_HEIGHT / 2 - (i + 1) * 1.5;
        enemies[i].vx = 0;
        enemies[i].vy = 0;
        enemies[i].angle = 0;
        enemies[i].lap = 0;
        enemies[i].finished = 0;
        enemies[i].symbol = symbols[i];
        enemies[i].checkpoint = 0;
        enemies[i].ai_offset = (i - 1) * 1.5;
    }
}

// Check if position is valid (not wall)
int is_valid_position(float x, float y) {
    int ix = (int)(x + 0.5);
    int iy = (int)(y + 0.5);
    
    if (ix < 0 || ix >= SCREEN_WIDTH || iy < 0 || iy >= SCREEN_HEIGHT) {
        return 0;
    }
    
    char cell = track[iy][ix];
    return cell != TRACK_WALL;
}

// Update checkpoint for lap counting
void update_checkpoint(Car *car) {
    int ix = (int)(car->x + 0.5);
    int iy = (int)(car->y + 0.5);
    
    // 4-checkpoint system for proper lap validation
    if (car->checkpoint == 0 && ix > SCREEN_WIDTH * 0.6) {
        car->checkpoint = 1;
    } else if (car->checkpoint == 1 && iy < SCREEN_HEIGHT * 0.35) {
        car->checkpoint = 2;
    } else if (car->checkpoint == 2 && ix < SCREEN_WIDTH * 0.4) {
        car->checkpoint = 3;
    } else if (car->checkpoint == 3 && ix < 10 && 
               iy > SCREEN_HEIGHT / 2 - 4 && iy < SCREEN_HEIGHT / 2 + 4) {
        car->lap++;
        car->checkpoint = 0;
        
        if (car->lap >= MAX_LAPS) {
            car->finished = 1;
            if (car == &player) {
                game_over = 1;
            }
        }
    }
}

// Update player car
void update_player(char input) {
    float acceleration = 0.25;
    float turn_speed = 0.12;
    float friction = 0.94;
    
    // Handle input
    if (input == 'w' || input == 'W' || input == 'i' || input == 'I') {
        player.vx += cos(player.angle) * acceleration;
        player.vy += sin(player.angle) * acceleration;
    }
    if (input == 's' || input == 'S' || input == 'k' || input == 'K') {
        player.vx -= cos(player.angle) * acceleration * 0.5;
        player.vy -= sin(player.angle) * acceleration * 0.5;
    }
    if (input == 'a' || input == 'A' || input == 'j' || input == 'J') {
        player.angle -= turn_speed;
    }
    if (input == 'd' || input == 'D' || input == 'l' || input == 'L') {
        player.angle += turn_speed;
    }
    
    // Apply friction
    player.vx *= friction;
    player.vy *= friction;
    
    // Limit speed
    float speed = sqrt(player.vx * player.vx + player.vy * player.vy);
    float max_speed = 1.8;
    if (speed > max_speed) {
        player.vx = (player.vx / speed) * max_speed;
        player.vy = (player.vy / speed) * max_speed;
    }
    
    // Update position with collision detection
    float new_x = player.x + player.vx;
    float new_y = player.y + player.vy;
    
    if (is_valid_position(new_x, new_y)) {
        player.x = new_x;
        player.y = new_y;
    } else {
        // Bounce off walls
        player.vx *= -0.3;
        player.vy *= -0.3;
    }
    
    update_checkpoint(&player);
}

// AI for enemy cars
void update_enemy(Car *enemy) {
    float target_x, target_y;
    
    // Waypoints based on checkpoint
    switch(enemy->checkpoint) {
        case 0:
            target_x = SCREEN_WIDTH * 0.75;
            target_y = SCREEN_HEIGHT / 2 + enemy->ai_offset;
            break;
        case 1:
            target_x = SCREEN_WIDTH * 0.75;
            target_y = SCREEN_HEIGHT * 0.25 + enemy->ai_offset;
            break;
        case 2:
            target_x = SCREEN_WIDTH * 0.25;
            target_y = SCREEN_HEIGHT * 0.25 + enemy->ai_offset;
            break;
        default:
            target_x = SCREEN_WIDTH * 0.25;
            target_y = SCREEN_HEIGHT / 2 + enemy->ai_offset;
    }
    
    // Calculate direction to target
    float dx = target_x - enemy->x;
    float dy = target_y - enemy->y;
    float target_angle = atan2(dy, dx);
    
    // Smooth turning
    float angle_diff = target_angle - enemy->angle;
    while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2 * M_PI;
    enemy->angle += angle_diff * 0.08;
    
    // Accelerate
    float acceleration = 0.22;
    enemy->vx += cos(enemy->angle) * acceleration;
    enemy->vy += sin(enemy->angle) * acceleration;
    
    // Apply friction
    enemy->vx *= 0.94;
    enemy->vy *= 0.94;
    
    // Speed limit
    float speed = sqrt(enemy->vx * enemy->vx + enemy->vy * enemy->vy);
    float max_speed = 1.5;
    if (speed > max_speed) {
        enemy->vx = (enemy->vx / speed) * max_speed;
        enemy->vy = (enemy->vy / speed) * max_speed;
    }
    
    // Update position
    float new_x = enemy->x + enemy->vx;
    float new_y = enemy->y + enemy->vy;
    
    if (is_valid_position(new_x, new_y)) {
        enemy->x = new_x;
        enemy->y = new_y;
    } else {
        enemy->vx *= -0.3;
        enemy->vy *= -0.3;
    }
    
    update_checkpoint(enemy);
}

// Render the game
void render() {
    clear_screen();
    
    // Create display buffer
    char display[SCREEN_HEIGHT][SCREEN_WIDTH + 1];
    memcpy(display, track, sizeof(track));
    
    // Draw enemies
    for (int i = 0; i < NUM_ENEMIES; i++) {
        int ex = (int)(enemies[i].x + 0.5);
        int ey = (int)(enemies[i].y + 0.5);
        if (ex >= 0 && ex < SCREEN_WIDTH && ey >= 0 && ey < SCREEN_HEIGHT) {
            display[ey][ex] = enemies[i].symbol;
        }
    }
    
    // Draw player
    int px = (int)(player.x + 0.5);
    int py = (int)(player.y + 0.5);
    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        display[py][px] = player.symbol;
    }
    
    // Print display
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        printf("%s\n", display[y]);
    }
    
    // Print status
    time_t current_time = time(NULL);
    int elapsed = (int)(current_time - start_time);
    float speed = sqrt(player.vx * player.vx + player.vy * player.vy);
    
    printf("════════════════════════════════════════════════════════════════════════════\n");
    printf(" Lap: %d/%d │ Time: %02d:%02d │ Speed: ", 
           player.lap, MAX_LAPS, elapsed / 60, elapsed % 60);
    
    // Speed bar
    int speed_bars = (int)(speed * 10);
    for (int i = 0; i < 15; i++) {
        printf(i < speed_bars ? "█" : "░");
    }
    printf(" │ WASD/IJKL: Drive │ Q: Quit\n");
    
    fflush(stdout);
}

// Victory screen
void show_victory() {
    clear_screen();
    time_t final_time = time(NULL);
    int elapsed = (int)(final_time - start_time);
    
    printf("\n\n\n");
    printf("     ╔════════════════════════════════════════════════════════╗\n");
    printf("     ║                                                        ║\n");
    printf("     ║          🏁  V I C T O R Y ! 🏁                        ║\n");
    printf("     ║                                                        ║\n");
    printf("     ║          You completed %d laps!                        ║\n", MAX_LAPS);
    printf("     ║                                                        ║\n");
    printf("     ║          Final Time: %02d:%02d                          ║\n", 
           elapsed / 60, elapsed % 60);
    printf("     ║                                                        ║\n");
    printf("     ║          Press any key to exit...                     ║\n");
    printf("     ║                                                        ║\n");
    printf("     ╚════════════════════════════════════════════════════════╝\n");
    printf("\n\n");
    
    fflush(stdout);
}

// Title screen
void show_title() {
    clear_screen();
    printf("\n\n");
    printf("     ╔════════════════════════════════════════════════════════╗\n");
    printf("     ║                                                        ║\n");
    printf("     ║         TOP-DOWN RACING CHAMPIONSHIP                  ║\n");
    printf("     ║                                                        ║\n");
    printf("     ║              Sprint Circuit Challenge                 ║\n");
    printf("     ║                                                        ║\n");
    printf("     ╠════════════════════════════════════════════════════════╣\n");
    printf("     ║                                                        ║\n");
    printf("     ║  OBJECTIVE: Complete %d laps before your opponents!   ║\n", MAX_LAPS);
    printf("     ║                                                        ║\n");
    printf("     ║  CONTROLS:                                             ║\n");
    printf("     ║    W or I - Accelerate                                 ║\n");
    printf("     ║    S or K - Brake                                      ║\n");
    printf("     ║    A or J - Turn Left                                  ║\n");
    printf("     ║    D or L - Turn Right                                 ║\n");
    printf("     ║    Q      - Quit Game                                  ║\n");
    printf("     ║                                                        ║\n");
    printf("     ║  You are 'P'. Opponents are 'A', 'B', 'C'.            ║\n");
    printf("     ║                                                        ║\n");
    printf("     ║              Press any key to start!                   ║\n");
    printf("     ║                                                        ║\n");
    printf("     ╚════════════════════════════════════════════════════════╝\n");
    printf("\n\n");
    fflush(stdout);
}

int main() {
    enable_raw_mode();
    hide_cursor();
    
    show_title();
    
    // Wait for key press
    while (getchar() == EOF) {
        usleep(10000);
    }
    
    init_track();
    init_cars();
    start_time = time(NULL);
    
    // Main game loop
    while (!game_over) {
        char input = getchar();
        if (input == 'q' || input == 'Q' || input == 27) { // 27 = ESC
            break;
        }
        
        update_player(input);
        
        for (int i = 0; i < NUM_ENEMIES; i++) {
            update_enemy(&enemies[i]);
        }
        
        render();
        usleep(FRAME_DELAY);
    }
    
    if (player.finished) {
        show_victory();
        while (getchar() == EOF) {
            usleep(10000);
        }
    }
    
    show_cursor();
    disable_raw_mode();
    clear_screen();
    
    return 0;
}
