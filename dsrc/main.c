#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
#endif

#define WIDTH 60
#define HEIGHT 24
#define MAX_SPEED 3.0f
#define ACCELERATION 0.3f
#define FRICTION 0.15f
#define TURN_SPEED 0.15f
#define NUM_RACERS 4

typedef struct {
    float x, y;
    float vx, vy;
    float angle;
    float speed;
    int lap;
    int checkpoint;
    char symbol;
    int color;
    int is_ai;
} Racer;

char track[HEIGHT][WIDTH];
Racer racers[NUM_RACERS];
int game_over = 0;
int winner = -1;

// Platform-specific functions
void clear_screen() {
    printf("\033[2J\033[H");
}

void set_color(int color) {
    printf("\033[%dm", color);
}

void reset_color() {
    printf("\033[0m");
}

#ifdef _WIN32
int kbhit_custom() {
    return _kbhit();
}

int getch_custom() {
    return _getch();
}

void sleep_ms(int ms) {
    Sleep(ms);
}
#else
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

int kbhit_custom() {
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    int ch = getchar();
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int getch_custom() {
    int ch = getchar();
    return ch;
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif

void init_track() {
    // Fill with grass
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            track[y][x] = '.';
        }
    }
    
    // Create oval track
    for(int y = 2; y < HEIGHT - 2; y++) {
        for(int x = 5; x < WIDTH - 5; x++) {
            float dx = (x - WIDTH/2) / 20.0f;
            float dy = (y - HEIGHT/2) / 8.0f;
            float dist = dx*dx + dy*dy;
            
            if(dist < 1.2f && dist > 0.7f) {
                track[y][x] = '#';
            }
        }
    }
    
    // Add start/finish line
    for(int y = HEIGHT/2 - 3; y <= HEIGHT/2 + 3; y++) {
        if(track[y][WIDTH - 15] == '#') {
            track[y][WIDTH - 15] = '|';
        }
    }
    
    // Add checkpoint
    for(int y = HEIGHT/2 - 3; y <= HEIGHT/2 + 3; y++) {
        if(track[y][14] == '#') {
            track[y][14] = '|';
        }
    }
}

int is_on_track(int x, int y) {
    if(x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
    return track[y][x] == '#' || track[y][x] == '|';
}

void init_racers() {
    char symbols[] = {'A', 'B', 'C', 'D'};
    int colors[] = {31, 32, 33, 36}; // Red, Green, Yellow, Cyan
    
    for(int i = 0; i < NUM_RACERS; i++) {
        racers[i].x = WIDTH - 17;
        racers[i].y = HEIGHT/2 - 2 + i;
        racers[i].vx = 0;
        racers[i].vy = 0;
        racers[i].angle = 3.14159f;
        racers[i].speed = 0;
        racers[i].lap = 0;
        racers[i].checkpoint = 0;
        racers[i].symbol = symbols[i];
        racers[i].color = colors[i];
        racers[i].is_ai = (i != 0);
    }
}

void update_racer(Racer* r, int up, int down, int left, int right) {
    // Acceleration
    if(up && r->speed < MAX_SPEED) {
        r->speed += ACCELERATION;
    }
    if(down && r->speed > -MAX_SPEED/2) {
        r->speed -= ACCELERATION;
    }
    
    // Turning
    if(left) {
        r->angle -= TURN_SPEED * (r->speed / MAX_SPEED);
    }
    if(right) {
        r->angle += TURN_SPEED * (r->speed / MAX_SPEED);
    }
    
    // Friction
    r->speed *= (1.0f - FRICTION);
    
    // Update velocity based on angle
    r->vx = cosf(r->angle) * r->speed;
    r->vy = sinf(r->angle) * r->speed;
    
    // Try to move
    float new_x = r->x + r->vx;
    float new_y = r->y + r->vy;
    
    // Check if on track
    if(is_on_track((int)new_x, (int)new_y)) {
        r->x = new_x;
        r->y = new_y;
    } else {
        // Hit wall, bounce back
        r->speed *= 0.3f;
    }
    
    // Check checkpoints
    int ix = (int)r->x;
    int iy = (int)r->y;
    
    if(track[iy][ix] == '|') {
        if(ix < WIDTH/2 && r->checkpoint == 0) {
            r->checkpoint = 1;
        } else if(ix > WIDTH/2 && r->checkpoint == 1) {
            r->checkpoint = 0;
            r->lap++;
        }
    }
}

void ai_control(Racer* r) {
    // Simple AI: follow the racing line
    float target_x, target_y;
    
    if(r->checkpoint == 0) {
        // Head to left checkpoint
        target_x = 14;
        target_y = HEIGHT/2;
    } else {
        // Head to finish line
        target_x = WIDTH - 15;
        target_y = HEIGHT/2;
    }
    
    float dx = target_x - r->x;
    float dy = target_y - r->y;
    float target_angle = atan2f(dy, dx);
    
    // Normalize angle difference
    float angle_diff = target_angle - r->angle;
    while(angle_diff > 3.14159f) angle_diff -= 2 * 3.14159f;
    while(angle_diff < -3.14159f) angle_diff += 2 * 3.14159f;
    
    int left = angle_diff < -0.1f;
    int right = angle_diff > 0.1f;
    int up = 1;
    int down = 0;
    
    // Slow down on turns
    if(fabsf(angle_diff) > 0.5f && r->speed > MAX_SPEED * 0.6f) {
        up = 0;
        down = 1;
    }
    
    update_racer(r, up, down, left, right);
}

void draw() {
    clear_screen();
    
    // Create display buffer
    char display[HEIGHT][WIDTH];
    int colors[HEIGHT][WIDTH];
    memcpy(display, track, sizeof(track));
    memset(colors, 0, sizeof(colors));
    
    // Draw racers
    for(int i = NUM_RACERS - 1; i >= 0; i--) {
        int x = (int)racers[i].x;
        int y = (int)racers[i].y;
        if(x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            display[y][x] = racers[i].symbol;
            colors[y][x] = racers[i].color;
        }
    }
    
    // Print display
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            if(colors[y][x] != 0) {
                set_color(colors[y][x]);
                printf("%c", display[y][x]);
                reset_color();
            } else {
                printf("%c", display[y][x]);
            }
        }
        printf("\n");
    }
    
    // Print stats
    printf("\n");
    for(int i = 0; i < NUM_RACERS; i++) {
        set_color(racers[i].color);
        printf("%c", racers[i].symbol);
        reset_color();
        printf(": Lap %d/3  Speed: %.1f  ", racers[i].lap, racers[i].speed);
        if(i % 2 == 1) printf("\n");
    }
    printf("\n");
    printf("Controls: Arrow Keys or WASD | Q to quit\n");
    
    if(game_over) {
        printf("\n*** RACE FINISHED! ***\n");
        set_color(racers[winner].color);
        printf("Winner: %c\n", racers[winner].symbol);
        reset_color();
    }
}

int main() {
    #ifndef _WIN32
    enable_raw_mode();
    #endif
    
    srand(time(NULL));
    init_track();
    init_racers();
    
    clear_screen();
    printf("SUPER OFF-ROAD RACING\n");
    printf("=====================\n\n");
    printf("You are racer A (Red)\n");
    printf("Complete 3 laps to win!\n");
    printf("Stay on the track (#)\n");
    printf("Pass through checkpoints (|)\n\n");
    printf("Press any key to start...\n");
    
    while(!kbhit_custom()) {
        sleep_ms(100);
    }
    getch_custom();
    
    int running = 1;
    while(running) {
        // Input
        int up = 0, down = 0, left = 0, right = 0;
        
        if(kbhit_custom()) {
            int ch = getch_custom();
            
            #ifdef _WIN32
            if(ch == 224) {
                ch = getch_custom();
                if(ch == 72) up = 1;
                if(ch == 80) down = 1;
                if(ch == 75) left = 1;
                if(ch == 77) right = 1;
            }
            #else
            if(ch == 27) {
                getch_custom();
                ch = getch_custom();
                if(ch == 'A') up = 1;
                if(ch == 'B') down = 1;
                if(ch == 'D') left = 1;
                if(ch == 'C') right = 1;
            }
            #endif
            
            if(ch == 'w' || ch == 'W') up = 1;
            if(ch == 's' || ch == 'S') down = 1;
            if(ch == 'a' || ch == 'A') left = 1;
            if(ch == 'd' || ch == 'D') right = 1;
            if(ch == 'q' || ch == 'Q') running = 0;
        }
        
        if(!game_over) {
            // Update player
            update_racer(&racers[0], up, down, left, right);
            
            // Update AI
            for(int i = 1; i < NUM_RACERS; i++) {
                ai_control(&racers[i]);
            }
            
            // Check for winner
            for(int i = 0; i < NUM_RACERS; i++) {
                if(racers[i].lap >= 3) {
                    game_over = 1;
                    winner = i;
                    break;
                }
            }
        }
        
        draw();
        sleep_ms(50);
    }
    
    clear_screen();
    printf("Thanks for playing!\n");
    
    return 0;
}
