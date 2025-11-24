#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define TRACK_WIDTH 80
#define TRACK_HEIGHT 30
#define MAX_CARS 4
#define TOTAL_LAPS 3
#define PI 3.14159265359

// Track elements
#define TRACK_WALL '#'
#define TRACK_ROAD ' '
#define TRACK_FINISH '='
#define TRACK_DIRT '.'

// Game structures
typedef struct {
    double x, y;
    double vx, vy;
    double angle;
    double speed;
    int lap;
    int checkpoint;
    char symbol;
    int is_ai;
    int finished;
    int finish_position;
} Car;

typedef struct {
    int x, y;
} Checkpoint;

// Global variables
char track[TRACK_HEIGHT][TRACK_WIDTH];
Car cars[MAX_CARS];
Checkpoint checkpoints[10];
int num_checkpoints = 0;
int game_over = 0;
int next_finish_pos = 1;

// Terminal handling
struct termios orig_termios;

void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void setup_terminal() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void hide_cursor() {
    printf("\033[?25l");
}

void show_cursor() {
    printf("\033[?25h");
}

// Initialize the track
void init_track() {
    // Fill with road
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        for (int x = 0; x < TRACK_WIDTH; x++) {
            track[y][x] = TRACK_ROAD;
        }
    }
    
    // Outer walls
    for (int x = 0; x < TRACK_WIDTH; x++) {
        track[0][x] = TRACK_WALL;
        track[TRACK_HEIGHT-1][x] = TRACK_WALL;
    }
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        track[y][0] = TRACK_WALL;
        track[y][TRACK_WIDTH-1] = TRACK_WALL;
    }
    
    // Inner obstacle (creates oval track)
    for (int y = 8; y < TRACK_HEIGHT - 8; y++) {
        for (int x = 25; x < TRACK_WIDTH - 25; x++) {
            track[y][x] = TRACK_WALL;
        }
    }
    
    // Add some dirt patches for challenge
    for (int i = 0; i < 30; i++) {
        int x = 10 + rand() % (TRACK_WIDTH - 20);
        int y = 5 + rand() % (TRACK_HEIGHT - 10);
        if (track[y][x] == TRACK_ROAD) {
            track[y][x] = TRACK_DIRT;
        }
    }
    
    // Finish line
    for (int x = 2; x < 10; x++) {
        track[TRACK_HEIGHT/2][x] = TRACK_FINISH;
    }
    
    // Setup checkpoints for lap counting
    num_checkpoints = 4;
    checkpoints[0].x = 5; checkpoints[0].y = TRACK_HEIGHT/2;  // Start
    checkpoints[1].x = 40; checkpoints[1].y = 5;               // Top
    checkpoints[2].x = 75; checkpoints[2].y = TRACK_HEIGHT/2;  // Right
    checkpoints[3].x = 40; checkpoints[3].y = TRACK_HEIGHT-5;  // Bottom
}

// Initialize cars
void init_cars() {
    int start_positions[MAX_CARS][2] = {
        {8, TRACK_HEIGHT/2 - 1},
        {8, TRACK_HEIGHT/2},
        {8, TRACK_HEIGHT/2 + 1},
        {7, TRACK_HEIGHT/2}
    };
    
    char symbols[MAX_CARS] = {'P', '1', '2', '3'};
    
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].x = start_positions[i][0];
        cars[i].y = start_positions[i][1];
        cars[i].vx = 0;
        cars[i].vy = 0;
        cars[i].angle = 0;
        cars[i].speed = 0;
        cars[i].lap = 0;
        cars[i].checkpoint = 0;
        cars[i].symbol = symbols[i];
        cars[i].is_ai = (i != 0);
        cars[i].finished = 0;
        cars[i].finish_position = 0;
    }
}

// Check if position is valid (not a wall)
int is_valid_position(double x, double y) {
    int ix = (int)(x + 0.5);
    int iy = (int)(y + 0.5);
    
    if (ix < 0 || ix >= TRACK_WIDTH || iy < 0 || iy >= TRACK_HEIGHT) {
        return 0;
    }
    
    return track[iy][ix] != TRACK_WALL;
}

// Get friction based on terrain
double get_friction(double x, double y) {
    int ix = (int)(x + 0.5);
    int iy = (int)(y + 0.5);
    
    if (ix < 0 || ix >= TRACK_WIDTH || iy < 0 || iy >= TRACK_HEIGHT) {
        return 0.95;
    }
    
    if (track[iy][ix] == TRACK_DIRT) {
        return 0.85; // More friction on dirt
    }
    
    return 0.95; // Normal road friction
}

// Update checkpoint progress
void update_checkpoint(Car *car) {
    if (car->finished) return;
    
    int next_checkpoint = (car->checkpoint + 1) % num_checkpoints;
    double dx = car->x - checkpoints[next_checkpoint].x;
    double dy = car->y - checkpoints[next_checkpoint].y;
    double dist = sqrt(dx*dx + dy*dy);
    
    if (dist < 5.0) {
        car->checkpoint = next_checkpoint;
        
        // Crossed finish line
        if (next_checkpoint == 0 && car->lap > 0) {
            car->lap++;
            if (car->lap > TOTAL_LAPS) {
                car->finished = 1;
                car->finish_position = next_finish_pos++;
            }
        } else if (next_checkpoint == 0) {
            car->lap = 1;
        }
    }
}

// Simple AI logic
void update_ai(Car *car) {
    if (car->finished) {
        car->speed *= 0.9;
        return;
    }
    
    int target_cp = (car->checkpoint + 1) % num_checkpoints;
    double target_x = checkpoints[target_cp].x;
    double target_y = checkpoints[target_cp].y;
    
    double dx = target_x - car->x;
    double dy = target_y - car->y;
    double target_angle = atan2(dy, dx);
    
    // Adjust angle towards target
    double angle_diff = target_angle - car->angle;
    while (angle_diff > PI) angle_diff -= 2*PI;
    while (angle_diff < -PI) angle_diff += 2*PI;
    
    if (angle_diff > 0.1) {
        car->angle += 0.08;
    } else if (angle_diff < -0.1) {
        car->angle -= 0.08;
    }
    
    // AI acceleration
    double target_speed = 1.5 + (rand() % 100) / 200.0;
    if (car->speed < target_speed) {
        car->speed += 0.15;
    }
    if (car->speed > target_speed) {
        car->speed -= 0.05;
    }
}

// Update car physics
void update_car(Car *car, int accel, int turn_left, int turn_right, int brake) {
    if (car->is_ai) {
        update_ai(car);
    } else {
        // Player controls
        if (turn_left) car->angle -= 0.12;
        if (turn_right) car->angle += 0.12;
        
        if (accel) {
            car->speed += 0.2;
            if (car->speed > 2.5) car->speed = 2.5;
        }
        
        if (brake) {
            car->speed -= 0.3;
            if (car->speed < 0) car->speed = 0;
        }
    }
    
    // Apply friction
    double friction = get_friction(car->x, car->y);
    car->speed *= friction;
    
    // Calculate velocity from angle and speed
    car->vx = cos(car->angle) * car->speed;
    car->vy = sin(car->angle) * car->speed;
    
    // Try to move
    double new_x = car->x + car->vx;
    double new_y = car->y + car->vy;
    
    // Collision detection
    if (is_valid_position(new_x, new_y)) {
        car->x = new_x;
        car->y = new_y;
    } else {
        // Hit wall - bounce back and lose speed
        car->speed *= 0.3;
        car->vx *= -0.5;
        car->vy *= -0.5;
    }
    
    // Keep in bounds
    if (car->x < 1) car->x = 1;
    if (car->x >= TRACK_WIDTH-1) car->x = TRACK_WIDTH-2;
    if (car->y < 1) car->y = 1;
    if (car->y >= TRACK_HEIGHT-1) car->y = TRACK_HEIGHT-2;
    
    update_checkpoint(car);
}

// Render the game
void render() {
    char display[TRACK_HEIGHT][TRACK_WIDTH];
    
    // Copy track to display
    memcpy(display, track, sizeof(track));
    
    // Draw cars
    for (int i = 0; i < MAX_CARS; i++) {
        int x = (int)(cars[i].x + 0.5);
        int y = (int)(cars[i].y + 0.5);
        if (x >= 0 && x < TRACK_WIDTH && y >= 0 && y < TRACK_HEIGHT) {
            display[y][x] = cars[i].symbol;
        }
    }
    
    // Print display
    clear_screen();
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        for (int x = 0; x < TRACK_WIDTH; x++) {
            putchar(display[y][x]);
        }
        putchar('\n');
    }
    
    // Print status
    printf("\n");
    printf("Player (P) - Lap: %d/%d | Speed: %.1f | ", 
           cars[0].lap, TOTAL_LAPS, cars[0].speed);
    
    if (cars[0].finished) {
        printf("FINISHED #%d!", cars[0].finish_position);
    }
    
    printf("\nControls: W=Accelerate, A/D=Turn, S=Brake, Q=Quit\n");
    
    // Show positions
    printf("\nPositions: ");
    for (int pos = 1; pos <= next_finish_pos - 1; pos++) {
        for (int i = 0; i < MAX_CARS; i++) {
            if (cars[i].finish_position == pos) {
                printf("#%d:%c ", pos, cars[i].symbol);
            }
        }
    }
    
    fflush(stdout);
}

// Main game loop
int main() {
    srand(time(NULL));
    
    setup_terminal();
    hide_cursor();
    clear_screen();
    
    init_track();
    init_cars();
    
    printf("TOP-DOWN RACING!\n");
    printf("Race %d laps around the track!\n", TOTAL_LAPS);
    printf("Press any key to start...\n");
    
    // Wait for keypress
    while (getchar() == EOF) {
        usleep(10000);
    }
    
    int accel = 0, turn_left = 0, turn_right = 0, brake = 0;
    
    while (!game_over) {
        // Input handling
        char c;
        accel = turn_left = turn_right = brake = 0;
        
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q') game_over = 1;
            if (c == 'w' || c == 'W') accel = 1;
            if (c == 's' || c == 'S') brake = 1;
            if (c == 'a' || c == 'A') turn_left = 1;
            if (c == 'd' || c == 'D') turn_right = 1;
        }
        
        // Update all cars
        for (int i = 0; i < MAX_CARS; i++) {
            if (i == 0) {
                update_car(&cars[i], accel, turn_left, turn_right, brake);
            } else {
                update_car(&cars[i], 0, 0, 0, 0);
            }
        }
        
        // Check if all cars finished
        int all_finished = 1;
        for (int i = 0; i < MAX_CARS; i++) {
            if (!cars[i].finished) {
                all_finished = 0;
                break;
            }
        }
        if (all_finished) game_over = 1;
        
        // Render
        render();
        
        // Frame delay (~30 FPS)
        usleep(33000);
    }
    
    clear_screen();
    show_cursor();
    
    printf("\n=== RACE COMPLETE ===\n\n");
    printf("Final Results:\n");
    for (int pos = 1; pos < next_finish_pos; pos++) {
        for (int i = 0; i < MAX_CARS; i++) {
            if (cars[i].finish_position == pos) {
                printf("  #%d: %c %s\n", pos, cars[i].symbol, 
                       i == 0 ? "(You)" : "(CPU)");
            }
        }
    }
    printf("\nThanks for playing!\n");
    
    return 0;
}
