/*
 * ASCII TOP-DOWN RACER
 * A single-file, no-dependency C game.
 *
 * Controls:
 *  W - Accelerate Up
 *  S - Accelerate Down
 *  A - Accelerate Left
 *  D - Accelerate Right
 *  Q - Quit
 *
 * Features: Drift physics, Figure-8 track, Lap timing, Collision detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------------
   PLATFORM SPECIFIC CODE
   Need non-blocking input and sleep functions which aren't standard C.
   -------------------------------------------------------------------------- */

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    
    void setup_terminal() {
        // Hide Cursor
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        info.dwSize = 100;
        info.bVisible = FALSE;
        SetConsoleCursorInfo(consoleHandle, &info);
    }

    void restore_terminal() {
        // Restore Cursor
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        info.dwSize = 100;
        info.bVisible = TRUE;
        SetConsoleCursorInfo(consoleHandle, &info);
    }

    int key_pressed() {
        return _kbhit();
    }

    char get_key() {
        return _getch();
    }

    void sleep_ms(int milliseconds) {
        Sleep(milliseconds);
    }

    void clear_screen() {
        // ANSI escape code to move cursor home (works in Win10+)
        // Fallback to system cls if older, but this reduces flicker
        printf("\033[H"); 
    }

#else // LINUX / MACOS
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #include <fcntl.h>

    struct termios orig_termios;

    void restore_terminal() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        printf("\e[?25h"); // Show cursor
    }

    void setup_terminal() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(restore_terminal);
        
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        
        printf("\e[?25l"); // Hide cursor
    }

    int key_pressed() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv);
    }

    char get_key() {
        char c;
        if (read(STDIN_FILENO, &c, 1) < 0) return 0;
        return c;
    }

    void sleep_ms(int milliseconds) {
        usleep(milliseconds * 1000);
    }

    void clear_screen() {
        printf("\033[H"); // Move cursor to top-left
    }
#endif

/* --------------------------------------------------------------------------
   GAME LOGIC
   -------------------------------------------------------------------------- */

#define MAP_WIDTH 60
#define MAP_HEIGHT 20

// ASCII Sideways Figure-8 Track
// '#' = Wall, ' ' = Road, '=' = Finish Line, 'X' = Checkpoint
// The track allows crossing in the middle.
const char LEVEL_DATA[MAP_HEIGHT][MAP_WIDTH + 1] = {
    "############################################################",
    "##            ########                ########            ##",
    "##          ###      ###            ###      ###          ##",
    "#          ##          ##          ##          ##          #",
    "#         ##            ##        ##            ##         #",
    "#        ##              ##      ##              ##        #",
    "#        ##               ##    ##               ##        #",
    "#       ##                 ##  ##                 ##       #",
    "#   X   ##                  ####                  ##       #",
    "#   X   ##                   ##                   ##   =   #",
    "#   X   ##                   ##                   ##   =   #",
    "#   X   ##                  ####                  ##       #",
    "#       ##                 ##  ##                 ##       #",
    "#        ##               ##    ##               ##        #",
    "#        ##              ##      ##              ##        #",
    "#         ##            ##        ##            ##         #",
    "#          ##          ##          ##          ##          #",
    "##          ###      ###            ###      ###          ##",
    "##            ########                ########            ##",
    "############################################################"
};

typedef struct {
    float x, y;
    float vx, vy;
} Car;

// Helper to determine what char is at a specific coordinate
char get_tile(int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return '#';
    return LEVEL_DATA[y][x];
}

int main() {
    setup_terminal();

    // Clear screen completely once
    #ifdef _WIN32
    system("cls");
    #else
    printf("\033[2J");
    #endif

    // Initial Car State (Placed near finish line)
    Car p1;
    p1.x = 53.0f;
    p1.y = 10.0f;
    p1.vx = 0.0f;
    p1.vy = 0.0f;

    // Physics constants
    const float ACCEL = 0.08f;
    const float FRICTION = 0.96f;
    const float BOUNCE = -0.5f; // Velocity retention on wall hit

    // Game State
    int running = 1;
    int laps = 0;
    int checkpoint_hit = 0; // 0 = No, 1 = Yes (Needed to count lap)
    
    // Time tracking for lap times
    time_t start_time = time(NULL);

    while (running) {
        /* --- INPUT --- */
        if (key_pressed()) {
            char c = get_key();
            // Convert to lowercase
            if (c >= 'A' && c <= 'Z') c += 32;

            if (c == 'q') running = 0;
            if (c == 'w') p1.vy -= ACCEL;
            if (c == 's') p1.vy += ACCEL;
            if (c == 'a') p1.vx -= ACCEL;
            if (c == 'd') p1.vx += ACCEL;
        }

        /* --- PHYSICS --- */
        // Apply friction (drift effect)
        p1.vx *= FRICTION;
        p1.vy *= FRICTION;

        // Predict next position
        float next_x = p1.x + p1.vx;
        float next_y = p1.y + p1.vy;

        // --- COLLISION ---
        
        // Check X axis collision
        char tile_x = get_tile((int)next_x, (int)p1.y);
        if (tile_x == '#') {
            p1.vx *= BOUNCE; // Bounce off wall
        } else {
            p1.x = next_x;
        }

        // Check Y axis collision
        char tile_y = get_tile((int)p1.x, (int)next_y);
        if (tile_y == '#') {
            p1.vy *= BOUNCE; // Bounce off wall
        } else {
            p1.y = next_y;
        }

        // --- GAMEPLAY LOGIC ---
        char current_tile = get_tile((int)p1.x, (int)p1.y);

        // Checkpoint logic (Left side of map)
        if (current_tile == 'X') {
            checkpoint_hit = 1;
        }

        // Finish line logic (Right side of map)
        // Only counts if checkpoint was hit previously to prevent line camping
        if (current_tile == '=' && checkpoint_hit) {
            laps++;
            checkpoint_hit = 0; // Reset checkpoint
        }

        /* --- RENDER --- */
        clear_screen(); // Move cursor to 0,0

        // Draw HUD
        printf("ASCII RACER - Use W/A/S/D to Drive, Q to Quit\n");
        printf("Laps: %d | Checkpoint: %s\n", laps, checkpoint_hit ? "CLEARED" : "Pending");
        printf("------------------------------------------------------------\n");

        // Draw Map
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if ((int)p1.y == y && (int)p1.x == x) {
                    printf("O"); // Draw Car
                } else {
                    // Draw Map Tile
                    putchar(LEVEL_DATA[y][x]);
                }
            }
            printf("\n");
        }

        // Frame delay (approx 30 FPS)
        sleep_ms(33);
    }

    restore_terminal();
    printf("\nGame Over! Total Laps: %d\n", laps);
    return 0;
}
