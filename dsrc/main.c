/*
    ASCII SUPER SPRINT RACER
    Single file, no external graphics dependencies.
    Compiles on Windows, Linux, and macOS.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

// --- CROSS PLATFORM UTILS ---
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    void sleep_ms(int ms) { Sleep(ms); }
    void clear_screen() { 
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD coord = {0, 0};
        DWORD count;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hOut, &csbi);
        FillConsoleOutputCharacter(hOut, ' ', csbi.dwSize.X * csbi.dwSize.Y, coord, &count);
        SetConsoleCursorPosition(hOut, coord);
    }
    void hide_cursor() {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        info.dwSize = 100;
        info.bVisible = FALSE;
        SetConsoleCursorInfo(consoleHandle, &info);
    }
    int key_hit() { return _kbhit(); }
    int get_key() { return _getch(); }
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>
    
    void sleep_ms(int ms) { usleep(ms * 1000); }
    void clear_screen() { printf("\033[2J\033[H"); } // ANSI clear
    void hide_cursor() { printf("\033[?25l"); }     // ANSI hide cursor
    void show_cursor() { printf("\033[?25h"); }     // ANSI show cursor
    
    int key_hit() {
        struct termios oldt, newt;
        int ch;
        int oldf;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        if(ch != EOF) {
            ungetc(ch, stdin);
            return 1;
        }
        return 0;
    }
    int get_key() { return getchar(); }
#endif

// --- GAME CONSTANTS ---
#define WIDTH 60
#define HEIGHT 24
#define FPS 20
#define TOTAL_LAPS 3
#define PI 3.1415926535
#define NUM_ENEMIES 2

// Map Tiles
#define WALL '#'
#define ROAD ' '
#define FINISH '='
#define CHECKPOINT '*'

// --- TRACK DATA ---
// 60x24 map
const char *TRACK_TEMPLATE[HEIGHT] = {
    "############################################################",
    "##                                                        ##",
    "##  ####################################################  ##",
    "##  #                                                  #  ##",
    "##  #  ##############################################  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  #      ########################              #  #  ##",
    "##  #  #      #                      #              #  #  ##",
    "##  #  #      #  ##################  #              #  #  ##",
    "##  #  #      #  #                #  #              #  #  ##",
    "##  #  #      #  #                #  #              #  #  ##",
    "##  #  #      #  ##################  #              #  #  ##",
    "##  #  #      #                      #              #  #  ##",
    "##  #  #      ########################              #  #  ##",
    "##  #  #                                            #  #  ##",
    "##  #  ##############################################  #  ##",
    "##  #                                                  #  ##",
    "##  ####################################################  ##",
    "## =                                                      ##",
    "############################################################"
};

// Waypoints for AI to follow (x, y)
// Simple path around the track
struct Point { float x; float y; };
struct Point waypoints[] = {
    {50, 21}, {55, 20}, {56, 3},  {50, 1},  {10, 1}, 
    {4, 3},   {4, 9},   {10, 11}, {20, 11}, {20, 16}, 
    {10, 16}, {5, 19},  {10, 22}, {30, 22}
};
int total_waypoints = 14;

// --- STRUCTURES ---
typedef struct {
    float x, y;
    float vx, vy;
    float angle; // Radians
    float speed;
    int laps;
    int next_waypoint_idx; // For AI
    int checkpoint_idx;    // To prevent cheating
    char sprite;
    int is_player;
    int finished;
} Car;

// --- GLOBAL STATE ---
char screen_buffer[HEIGHT][WIDTH + 1]; // +1 for null terminator
Car player;
Car enemies[NUM_ENEMIES];
long start_time;

// --- FUNCTIONS ---

void init_game() {
    // Setup Player
    player.x = 10; player.y = 22;
    player.vx = 0; player.vy = 0;
    player.angle = 0; 
    player.speed = 0;
    player.laps = 0;
    player.checkpoint_idx = 0;
    player.sprite = 'P';
    player.is_player = 1;
    player.finished = 0;

    // Setup Enemies
    for(int i=0; i<NUM_ENEMIES; i++) {
        enemies[i].x = 8 - (i*2); 
        enemies[i].y = 22;
        enemies[i].vx = 0; enemies[i].vy = 0;
        enemies[i].angle = 0;
        enemies[i].speed = 0;
        enemies[i].laps = 0;
        enemies[i].next_waypoint_idx = 13; // Start aiming for end of straight
        enemies[i].sprite = 'E';
        enemies[i].is_player = 0;
        enemies[i].finished = 0;
    }

    start_time = time(NULL);
    hide_cursor();
}

// Check collision with walls
int is_wall(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 1;
    return TRACK_TEMPLATE[y][x] == WALL;
}

// Handle lap counting logic
void check_lap(Car *c) {
    // Simple checkpoint system based on regions
    // Checkpoint 0: Start line area (Left bottom)
    // Checkpoint 1: Top Right area
    // Checkpoint 2: Middle loop
    
    int cx = (int)c->x;
    int cy = (int)c->y;

    // Region definitions
    int region = -1;
    if (cy > 20 && cx < 15) region = 0; // Start/Finish area
    else if (cy < 5 && cx > 40) region = 1; // Top right
    else if (cy > 10 && cy < 17 && cx > 15 && cx < 25) region = 2; // Inner loop

    // Progression logic
    if (region == 1 && c->checkpoint_idx == 0) c->checkpoint_idx = 1;
    if (region == 2 && c->checkpoint_idx == 1) c->checkpoint_idx = 2;
    
    // Finish Lap (Crossing line at x=5, y=22 approx)
    if (region == 0 && c->checkpoint_idx == 2) {
        c->laps++;
        c->checkpoint_idx = 0;
    }
}

void update_physics(Car *c) {
    if (c->finished) return;

    // Apply Velocity
    c->vx += cos(c->angle) * c->speed;
    c->vy += sin(c->angle) * c->speed;

    // Predict next position
    float next_x = c->x + c->vx;
    float next_y = c->y + c->vy;

    // Wall Collision
    if (is_wall((int)next_x, (int)next_y)) {
        // Bounce / Stop
        c->vx *= -0.5;
        c->vy *= -0.5;
        c->speed = 0;
    } else {
        c->x = next_x;
        c->y = next_y;
    }

    // Friction (Drift effect)
    c->vx *= 0.85;
    c->vy *= 0.85;
    c->speed = 0; // Speed is added impulse, reset per frame

    check_lap(c);

    if (c->laps >= TOTAL_LAPS) c->finished = 1;
}

void update_ai(Car *c) {
    if (c->finished) return;

    struct Point target = waypoints[c->next_waypoint_idx];
    
    // Distance to waypoint
    float dx = target.x - c->x;
    float dy = target.y - c->y;
    float dist = sqrt(dx*dx + dy*dy);

    // Target angle
    float target_angle = atan2(dy, dx);
    
    // Smooth steering
    float diff = target_angle - c->angle;
    // Normalize angle
    while (diff <= -PI) diff += 2*PI;
    while (diff > PI) diff -= 2*PI;

    if (diff > 0.1) c->angle += 0.15;
    else if (diff < -0.1) c->angle -= 0.15;
    
    // Accelerate
    c->speed = 0.35; // Constant speed for AI

    // Check if reached waypoint
    if (dist < 4.0) {
        c->next_waypoint_idx++;
        if (c->next_waypoint_idx >= total_waypoints) c->next_waypoint_idx = 0;
    }
}

void render() {
    // Reset cursor to top-left (avoids flickering compared to clear screen)
    #ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD coord = {0, 0};
        SetConsoleCursorPosition(hOut, coord);
    #else
        printf("\033[H"); 
    #endif

    // 1. Draw Map to Buffer
    for(int y=0; y<HEIGHT; y++) {
        strcpy(screen_buffer[y], TRACK_TEMPLATE[y]);
    }

    // 2. Draw Enemies
    for(int i=0; i<NUM_ENEMIES; i++) {
        int ex = (int)enemies[i].x;
        int ey = (int)enemies[i].y;
        if(ex >=0 && ex < WIDTH && ey >=0 && ey < HEIGHT) {
            screen_buffer[ey][ex] = enemies[i].sprite;
        }
    }

    // 3. Draw Player
    int px = (int)player.x;
    int py = (int)player.y;
    if(px >=0 && px < WIDTH && py >=0 && py < HEIGHT) {
        // Directional Character
        char p_char = '>';
        float a = player.angle;
        while(a < 0) a+= 2*PI;
        while(a > 2*PI) a-= 2*PI;
        
        if(a >= PI/4 && a < 3*PI/4) p_char = 'v';
        else if(a >= 3*PI/4 && a < 5*PI/4) p_char = '<';
        else if(a >= 5*PI/4 && a < 7*PI/4) p_char = '^';
        
        screen_buffer[py][px] = p_char;
    }

    // 4. Print Buffer
    printf("ASCII SUPER SPRINT - LAP %d/%d - TIME: %ld s\n", 
           player.laps, TOTAL_LAPS, time(NULL) - start_time);
    
    for(int y=0; y<HEIGHT; y++) {
        printf("%s\n", screen_buffer[y]);
    }
    
    printf("Controls: W (Gas), A/D (Steer). Don't hit walls!\n");
    if (player.finished) {
        printf("\n*** RACE FINISHED! ***\n");
    }
}

int main() {
    init_game();
    clear_screen(); // Clear once at start

    int running = 1;
    
    while(running) {
        // Input
        if(key_hit()) {
            char key = get_key();
            if(key == 'w' || key == 'W') player.speed = 0.5; // Accelerate
            if(key == 'a' || key == 'A') player.angle -= 0.2;
            if(key == 'd' || key == 'D') player.angle += 0.2;
            if(key == 'q') running = 0;
        }

        // Updates
        update_physics(&player);
        
        for(int i=0; i<NUM_ENEMIES; i++) {
            update_ai(&enemies[i]);
            update_physics(&enemies[i]);
        }

        // Render
        render();

        // End Game Check
        if (player.finished) {
            int enemies_done = 1;
            for(int i=0; i<NUM_ENEMIES; i++) if(!enemies[i].finished) enemies_done = 0;
            
            // If player wins
            printf("\n\n");
            printf("##############################\n");
            printf("#      VICTORY! YOU WON!     #\n");
            printf("##############################\n");
            
            // Simple wait loop before exit
            sleep_ms(3000);
            break;
        }

        // Frame Limiter
        sleep_ms(1000 / FPS);
    }

    #ifndef _WIN32
    show_cursor();
    #endif
    return 0;
}
