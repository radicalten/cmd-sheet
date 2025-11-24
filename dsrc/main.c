/*
 * ASCII RACER - Single File C Game
 * No external library dependencies (uses Standard C + platform specific headers for IO).
 *
 * CONTROLS: W, A, S, D to move. Q to Quit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// -----------------------------------------------------------------------------
// PLATFORM SPECIFIC SETUP (For Input and Sleep)
// -----------------------------------------------------------------------------
#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    void sleep_ms(int milliseconds) { Sleep(milliseconds); }
    void clear_screen() { system("cls"); } // Fallback for older cmd, though ANSI is preferred
    void setup_console() {
        // Enable ANSI escape codes on Windows 10+
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, dwMode);
    }
    void restore_console() {}
    int kbhit_custom() { return _kbhit(); }
    char getch_custom() { return _getch(); }
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/select.h>

    void sleep_ms(int milliseconds) { usleep(milliseconds * 1000); }
    
    struct termios orig_termios;

    void restore_console() {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        printf("\e[?25h"); // Show cursor
    }

    void setup_console() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(restore_console);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        printf("\e[?25l"); // Hide cursor
    }

    int kbhit_custom() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv) > 0;
    }

    char getch_custom() {
        char c;
        if (read(0, &c, 1) < 0) return 0;
        return c;
    }
#endif

// -----------------------------------------------------------------------------
// GAME CONSTANTS & DATA
// -----------------------------------------------------------------------------
#define WIDTH 40
#define HEIGHT 20
#define LAPS_TO_WIN 3
#define FPS 15

// 0=Empty, 1=Wall, 2=FinishLine, 3=Checkpoint
int track_logic[HEIGHT][WIDTH]; 

// Visual Map
const char *track_design[] = {
    "########################################",
    "#                                      #",
    "#  ##################################  #",
    "#  #                                #  #",
    "#  #  ########    ############   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "S  #  #      #    #          #   #  #  #",
    "S  #  #      #    #          #   #  #  #",
    "S  #  #      #    #          #   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "#  #  #      #    #          #   #  #  #",
    "#  #  ########    ############   #  #  #",
    "#  #                             #  #  #",
    "#  ###############################  #  #",
    "#                                   #  #",
    "#                                      #",
    "########################################"
};

// Simple waypoints for Enemy AI to follow (x, y)
int waypoints[][2] = {
    {3, 17}, {35, 17}, {35, 1}, {3, 1}, {3, 8} // Loops around
};
int total_waypoints = 5;

typedef struct {
    float x, y;
    float speed;
    int lap;
    int checkpoint_idx; // 0: start, 1: mid-track logic
    int current_wp; // for AI
    char symbol;
} Car;

Car player;
Car enemy;
long start_time;
int game_over = 0;
int victory = 0;
char screen_buffer[HEIGHT * (WIDTH + 1) + 100]; // Buffer for rendering

// -----------------------------------------------------------------------------
// LOGIC
// -----------------------------------------------------------------------------

void init_game() {
    // Initialize Track Logic
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            char c = track_design[y][x];
            if (c == '#') track_logic[y][x] = 1;
            else if (c == 'S') track_logic[y][x] = 2; // Start/Finish
            else track_logic[y][x] = 0;
            
            // Add invisible checkpoints based on coordinates
            if (x > 30 && y > 5 && y < 15) track_logic[y][x] = 3; // Right side checkpoint
        }
    }

    // Setup Player
    player.x = 5; player.y = 8;
    player.speed = 1.0; // Grid units per frame
    player.lap = 0;
    player.checkpoint_idx = 0;
    player.symbol = 'P';

    // Setup Enemy
    enemy.x = 5; enemy.y = 6;
    enemy.speed = 0.6; // Slower than player
    enemy.current_wp = 0;
    enemy.symbol = 'E';

    start_time = time(NULL);
}

void move_car(Car *c, float dx, float dy) {
    float new_x = c->x + dx;
    float new_y = c->y + dy;

    // Collision with Walls
    if (track_logic[(int)new_y][(int)new_x] != 1) {
        c->x = new_x;
        c->y = new_y;
    }

    // Logic for Laps (Specific to Player usually, but good for consistency)
    int tile = track_logic[(int)c->y][(int)c->x];
    
    // Checkpoint logic (must pass invisible checkpoint on right to count lap on left)
    if (tile == 3) {
        c->checkpoint_idx = 1;
    }
    
    if (tile == 2 && c->checkpoint_idx == 1) {
        // To prevent rapid lap counting, ensure we are moving "up" or generally correct direction
        // Here we just check if we have the checkpoint flag
        c->lap++;
        c->checkpoint_idx = 0; // Reset checkpoint
    }
}

void update_enemy() {
    // Simple AI: Move towards current waypoint
    int tx = waypoints[enemy.current_wp][0];
    int ty = waypoints[enemy.current_wp][1];

    float dx = 0, dy = 0;
    if ((int)enemy.x < tx) dx = enemy.speed;
    else if ((int)enemy.x > tx) dx = -enemy.speed;
    
    if ((int)enemy.y < ty) dy = enemy.speed;
    else if ((int)enemy.y > ty) dy = -enemy.speed;

    // If close to waypoint, switch to next
    if (abs((int)enemy.x - tx) <= 1 && abs((int)enemy.y - ty) <= 1) {
        enemy.current_wp++;
        if (enemy.current_wp >= total_waypoints) enemy.current_wp = 0;
    }

    move_car(&enemy, dx, dy);
}

void render() {
    // Move cursor to top-left (ANSI)
    printf("\033[H");

    // Build Frame
    char frame[HEIGHT][WIDTH + 1];

    // 1. Draw Map
    for (int y = 0; y < HEIGHT; y++) {
        strcpy(frame[y], track_design[y]);
    }

    // 2. Draw Enemy
    if(frame[(int)enemy.y][(int)enemy.x] != '#')
        frame[(int)enemy.y][(int)enemy.x] = enemy.symbol;

    // 3. Draw Player
    if(frame[(int)player.y][(int)player.x] != '#')
        frame[(int)player.y][(int)player.x] = player.symbol;

    // 4. Draw HUD
    printf("ASCII RACER | WASD to Drive | Q to Quit\n");
    printf("----------------------------------------\n");
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", frame[y]);
    }
    
    int elapsed = (int)(time(NULL) - start_time);
    printf("----------------------------------------\n");
    printf("LAP: %d / %d   TIME: %02d:%02d\n", player.lap, LAPS_TO_WIN, elapsed / 60, elapsed % 60);
}

void render_victory() {
    clear_screen();
    printf("\n\n");
    printf("########################################\n");
    printf("#                                      #\n");
    printf("#           VICTORY !!!                #\n");
    printf("#                                      #\n");
    printf("#      You finished %d laps!           #\n", LAPS_TO_WIN);
    printf("#      Total Time: %ld seconds          #\n", time(NULL) - start_time);
    printf("#                                      #\n");
    printf("########################################\n");
    printf("\nPress Q to exit...\n");
}

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------

int main() {
    setup_console();
    init_game();
    
    // Clear screen once at start
    printf("\033[2J"); 

    while (!game_over) {
        // 1. Input
        float dx = 0, dy = 0;
        if (kbhit_custom()) {
            char c = getch_custom();
            if (c == 'q' || c == 'Q') game_over = 1;
            if (c == 'w' || c == 'W') dy = -player.speed;
            if (c == 's' || c == 'S') dy = player.speed;
            if (c == 'a' || c == 'A') dx = -player.speed;
            if (c == 'd' || c == 'D') dx = player.speed;
        }

        // 2. Update
        move_car(&player, dx, dy);
        update_enemy();

        // Check Collision between Player and Enemy
        if ((int)player.x == (int)enemy.x && (int)player.y == (int)enemy.y) {
            // Penalty: Push player back
            player.x = 5; 
            player.y = 8;
        }

        // Check Win
        if (player.lap >= LAPS_TO_WIN) {
            victory = 1;
            game_over = 1;
        }

        // 3. Draw
        if (!victory) render();

        // 4. Wait
        sleep_ms(1000 / FPS);
    }

    if (victory) {
        render_victory();
        while(1) {
            if (kbhit_custom()) {
                char c = getch_custom();
                if (c == 'q' || c == 'Q') break;
            }
        }
    }

    restore_console();
    return 0;
}
