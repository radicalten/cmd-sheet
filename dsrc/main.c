/*
 * ASCII SUPER SPRINT / OFF ROAD RACER
 * Single file, no external dependencies (uses standard system headers).
 * Works on Windows (MinGW/MSVC) and Linux/macOS (GCC/Clang).
 *
 * CONTROLS:
 *   W / Up Arrow    : Accelerate
 *   A / Left Arrow  : Turn Left
 *   D / Right Arrow : Turn Right
 *   Q               : Quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

/* --- PLATFORM SPECIFIC SETUP --- */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <conio.h>
    #define PLATFORM_WINDOWS
    
    void setup_console() {
        // Hide Cursor
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        info.dwSize = 100;
        info.bVisible = FALSE;
        SetConsoleCursorInfo(consoleHandle, &info);
    }

    void clear_screen_buffer() {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD cursorCoord = {0, 0};
        SetConsoleCursorPosition(consoleHandle, cursorCoord);
    }

    void sleep_ms(int ms) {
        Sleep(ms);
    }

    int key_pressed() {
        return _kbhit();
    }

    int get_key() {
        return _getch();
    }

#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <sys/time.h>
    #define PLATFORM_LINUX

    void setup_console() {
        // Hide cursor and clear screen
        printf("\033[?25l"); 
        printf("\033[2J");
    }

    // Non-blocking keyboard input setup for Linux
    int key_pressed() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv) > 0;
    }

    int get_key() {
        int r;
        unsigned char c;
        if ((r = read(0, &c, sizeof(c))) < 0) return r;
        return c;
    }

    void clear_screen_buffer() {
        // Move cursor to top left
        printf("\033[H"); 
    }

    void sleep_ms(int ms) {
        usleep(ms * 1000);
    }
    
    // Set terminal to raw mode to get input without Enter
    struct termios orig_termios;
    void reset_terminal_mode() {
        tcsetattr(0, TCSANOW, &orig_termios);
        printf("\033[?25h"); // Show cursor
    }
    
    void set_conio_terminal_mode() {
        struct termios new_termios;
        tcgetattr(0, &orig_termios);
        memcpy(&new_termios, &orig_termios, sizeof(new_termios));
        atexit(reset_terminal_mode);
        cfmakeraw(&new_termios);
        tcsetattr(0, TCSANOW, &new_termios);
    }
#endif

/* --- GAME CONSTANTS & MATH --- */
#define PI 3.14159265
#define WIDTH 60
#define HEIGHT 30
#define FPS 30

// Physics Tuning
#define ACCEL 0.05f
#define MAX_SPEED 1.2f
#define TURN_SPEED 0.15f
#define FRICTION 0.96f
#define WALL_BOUNCE -0.5f

typedef struct {
    float x, y;
    float vx, vy;
    float angle; // Radians
    int color;   // Not fully utilized in ASCII, but good for expansion
    int laps;
    int last_checkpoint;
    char symbol;
} Car;

// ASCII Track Layout (60x30)
// # = Wall, . = Road, 1-4 = Checkpoints
const char TRACK_DATA[HEIGHT][WIDTH+1] = {
    "############################################################",
    "#..........................................................#",
    "#...####################################################...#",
    "#...#1.................................................#...#",
    "#...#..................................................#...#",
    "#...#.......####################################.......#...#",
    "#...#.......#..................................#.......#...#",
    "#...#.......#..................................#.......#...#",
    "#...#.......#.......####################.......#.......#...#",
    "#...#.......#.......#..................#.......#.......#...#",
    "#...#.......#.......#..................#.......#.......#...#",
    "#...#.......#.......#...############...#.......#.......#...#",
    "#...#.......#.......#...#..........#...#.......#.......#...#",
    "#...#.......#.......#...#..........#...#.......#.......#...#",
    "#2..#.......#.......#...#..........#...#.......#.......#..3#",
    "#...#.......#.......#...#..........#...#.......#.......#...#",
    "#...#.......#.......#...#..........#...#.......#.......#...#",
    "#...#.......#.......#...#..........#...#.......#.......#...#",
    "#...#.......#.......#...############...#.......#.......#...#",
    "#...#.......#.......#..................#.......#.......#...#",
    "#...#.......#.......#..................#.......#.......#...#",
    "#...#.......#.......####################.......#.......#...#",
    "#...#.......#..................................#.......#...#",
    "#...#.......#..................................#.......#...#",
    "#...#.......####################################.......#...#",
    "#...#..................................................#...#",
    "#...#.........................4........................#...#",
    "#...####################################################...#",
    "#.............................=............................#", // = is Start
    "############################################################"
};

// AI Waypoints (simple x,y targets)
float waypoints[][2] = {
    {30, 28}, {55, 28}, {55, 14}, {55, 2}, {30, 2}, {4, 2}, {4, 14}, {4, 28}
};
int num_waypoints = 8;

/* --- LOGIC --- */

// Get character based on angle for visual flair
char get_car_char(float angle) {
    // Normalize angle 0 to 2PI
    while (angle < 0) angle += 2 * PI;
    while (angle >= 2 * PI) angle -= 2 * PI;
    
    // 8 directions
    int dir = (int)((angle + PI/8.0) / (PI/4.0)) % 8;
    
    char chars[] = {'>', 'v', 'v', '<', '<', '^', '^', '>'}; 
    // Note: Diagonals in ASCII are tricky, sticking to simplified set for clarity
    // Or detailed: > \ v / < \ ^ /
    char detailed[] = {'>', '\\', 'v', '/', '<', '\\', '^', '/'};
    return detailed[dir];
}

void update_physics(Car *c, int input_up, int input_left, int input_right) {
    // Steering
    if (input_left) c->angle -= TURN_SPEED;
    if (input_right) c->angle += TURN_SPEED;

    // Acceleration (Vector math)
    if (input_up) {
        c->vx += cos(c->angle) * ACCEL;
        c->vy += sin(c->angle) * ACCEL;
    }

    // Friction (Drifting feel)
    c->vx *= FRICTION;
    c->vy *= FRICTION;

    // Cap Speed
    float speed = sqrt(c->vx*c->vx + c->vy*c->vy);
    if (speed > MAX_SPEED) {
        c->vx = (c->vx / speed) * MAX_SPEED;
        c->vy = (c->vy / speed) * MAX_SPEED;
    }

    // Predict next position
    float next_x = c->x + c->vx;
    float next_y = c->y + c->vy;

    // Collision Detection (Grid based)
    int grid_x = (int)next_x;
    int grid_y = (int)next_y;

    if (grid_x >= 0 && grid_x < WIDTH && grid_y >= 0 && grid_y < HEIGHT) {
        if (TRACK_DATA[grid_y][grid_x] == '#') {
            // Bounce!
            c->vx *= WALL_BOUNCE;
            c->vy *= WALL_BOUNCE;
            // Push out slightly to prevent getting stuck
            c->x += c->vx;
            c->y += c->vy;
        } else {
            // Move
            c->x = next_x;
            c->y = next_y;
        }
    }
}

void check_laps(Car *c) {
    int gx = (int)c->x;
    int gy = (int)c->y;
    char tile = TRACK_DATA[gy][gx];

    // Simple checkpoint system
    if (tile >= '1' && tile <= '4') {
        int cp = tile - '0';
        if (cp == c->last_checkpoint + 1) {
            c->last_checkpoint = cp;
        }
    }
    
    // Start line logic (simplified for ASCII)
    // In track data, '=' is the start line area roughly at (30, 28)
    if (tile == '=' && c->last_checkpoint == 4) {
        c->laps++;
        c->last_checkpoint = 0;
    }
}

void update_ai(Car *ai, Car *player) {
    // Find target waypoint
    float tx = waypoints[ai->last_checkpoint % num_waypoints][0];
    float ty = waypoints[ai->last_checkpoint % num_waypoints][1];

    // Distance to target
    float dx = tx - ai->x;
    float dy = ty - ai->y;
    float dist = sqrt(dx*dx + dy*dy);

    // If close, target next
    if (dist < 4.0f) {
        ai->last_checkpoint++;
    }

    // Desired angle
    float target_angle = atan2(dy, dx);
    
    // Basic steering logic to face target
    float diff = target_angle - ai->angle;
    while (diff <= -PI) diff += 2*PI;
    while (diff > PI) diff -= 2*PI;

    int left = 0, right = 0, up = 1; // Always gas
    if (diff > 0.1) right = 1;
    if (diff < -0.1) left = 1;

    // AI slows down if turning hard
    if (fabs(diff) > 1.0) up = 0; 

    update_physics(ai, up, left, right);
}

/* --- MAIN --- */

int main() {
    #ifdef PLATFORM_LINUX
    set_conio_terminal_mode();
    #endif
    setup_console();

    Car player = {30.0f, 28.0f, 0, 0, 0, 1, 0, 0, 'P'}; // Angle 0 = Right
    Car cpu    = {28.0f, 28.0f, 0, 0, 0, 2, 0, 0, 'C'};

    // Screen Buffer
    char buffer[HEIGHT * (WIDTH + 1) + 1]; 

    int running = 1;
    
    while (running) {
        // 1. Input
        int up=0, left=0, right=0;
        
        if (key_pressed()) {
            int k = get_key();
            #ifdef PLATFORM_WINDOWS
            // Windows arrow keys return 224 then code
            if (k == 224) { 
                k = get_key();
                if (k == 72) up = 1;    // Up
                if (k == 75) left = 1;  // Left
                if (k == 77) right = 1; // Right
            }
            #else
            // Linux/ANSI escape codes (simplified)
            if (k == 27 && key_pressed()) {
                 get_key(); // skip [
                 int arrow = get_key();
                 if (arrow == 'A') up = 1;
                 if (arrow == 'D') left = 1;
                 if (arrow == 'C') right = 1;
            }
            #endif
            
            // WASD support
            if (k == 'w' || k == 'W') up = 1;
            if (k == 'a' || k == 'A') left = 1;
            if (k == 'd' || k == 'D') right = 1;
            if (k == 'q' || k == 'Q') running = 0;
        }

        // 2. Update
        update_physics(&player, up, left, right);
        check_laps(&player);

        // AI runs automatically
        update_ai(&cpu, &player); // CPU just chases waypoints, doesn't react to player

        // 3. Render
        clear_screen_buffer();
        
        // Fill buffer with track
        int b_idx = 0;
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                buffer[b_idx++] = TRACK_DATA[y][x];
            }
            buffer[b_idx++] = '\n';
        }
        buffer[b_idx] = '\0';

        // Draw Cars into buffer
        auto draw_car = [&](Car *c, char symbol) {
            int cx = (int)c->x;
            int cy = (int)c->y;
            if (cx >= 0 && cx < WIDTH && cy >= 0 && cy < HEIGHT) {
                // Calculate index in buffer: y * (WIDTH + newline) + x
                int idx = cy * (WIDTH + 1) + cx;
                buffer[idx] = get_car_char(c->angle);
                // If you want a specific letter instead of arrow:
                // buffer[idx] = symbol; 
            }
        };

        // Since we are in C, we can't use lambdas easily without C++, 
        // so we just inline the logic or use helper function logic here.
        // Player
        int px = (int)player.x;
        int py = (int)player.y;
        if (px >=0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            buffer[py * (WIDTH + 1) + px] = get_car_char(player.angle);
        }
        // CPU
        int cx = (int)cpu.x;
        int cy = (int)cpu.y;
        if (cx >=0 && cx < WIDTH && cy >= 0 && cy < HEIGHT) {
             // Make CPU look different or same
             // If overlap, player draws on top
             if (cx != px || cy != py)
                buffer[cy * (WIDTH + 1) + cx] = 'C';
        }

        // Print Frame
        printf("%s", buffer);
        
        // HUD
        printf("PLAYER LAPS: %d  |  CPU LAPS: %d  |  Q to Quit\n", player.laps, cpu.laps/8); // CPU laps logic hacky due to waypoint index usage

        // 4. Framerate cap
        sleep_ms(1000 / FPS);
    }

    #ifdef PLATFORM_LINUX
    printf("\033[?25h"); // Restore cursor
    #endif
    return 0;
}
