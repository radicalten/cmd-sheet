#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define SLEEP(ms) Sleep(ms)
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <sys/select.h>
    #define SLEEP(ms) usleep((ms)*1000)
#endif

#define WIDTH 70
#define HEIGHT 22
#define TRACK_POINTS 40
#define M_PI 3.14159265358979323846

// ANSI Colors
#define CLR_RESET "\033[0m"
#define CLR_GREEN "\033[32m"
#define CLR_BROWN "\033[33m"
#define CLR_RED "\033[91m"
#define CLR_YELLOW "\033[93m"
#define CLR_GRAY "\033[90m"
#define CLR_CYAN "\033[96m"

typedef struct {
    float x, y, vx, vy, angle;
    int lap, checkpoint;
} Car;

char screen[HEIGHT][WIDTH];
Car player;
int gameRunning = 1;
int totalLaps = 3;
clock_t startTime;

#ifndef _WIN32
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(0, TCSAFLUSH, &orig_termios);
    printf("\033[?25h"); // Show cursor
}

void enableRawMode() {
    tcgetattr(0, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSAFLUSH, &raw);
    printf("\033[?25l"); // Hide cursor
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
#else
void enableRawMode() { 
    printf("\033[?25l"); // Hide cursor
}
void disableRawMode() { 
    printf("\033[?25h"); // Show cursor
}
#endif

void clearScreen() {
    printf("\033[2J\033[H");
}

void initCar() {
    player.x = WIDTH / 2.0f;
    player.y = HEIGHT / 2.0f - 6.5f;
    player.vx = 0;
    player.vy = 0;
    player.angle = M_PI / 2;
    player.lap = 0;
    player.checkpoint = 0;
}

int isOnTrack(int x, int y) {
    float cx = WIDTH / 2.0f;
    float cy = HEIGHT / 2.0f;
    float dx = (x - cx) / 2.5f;
    float dy = (y - cy);
    float dist = sqrt(dx * dx + dy * dy);
    return (dist < 9.5f && dist > 5.0f);
}

void drawTrack() {
    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    
    // Draw background
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            screen[y][x] = ((x + y) % 3 == 0) ? ',' : ' ';
        }
    }
    
    // Draw track surface
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (isOnTrack(x, y)) {
                screen[y][x] = ((x * 2 + y) % 3 == 0) ? '.' : ':';
            }
        }
    }
    
    // Draw outer wall
    for (int i = 0; i < TRACK_POINTS; i++) {
        float angle = (i * 2.0f * M_PI) / TRACK_POINTS;
        int x = cx + (int)(cos(angle) * 24);
        int y = cy + (int)(sin(angle) * 9.5f);
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            screen[y][x] = '#';
    }
    
    // Draw inner wall
    for (int i = 0; i < TRACK_POINTS; i++) {
        float angle = (i * 2.0f * M_PI) / TRACK_POINTS;
        int x = cx + (int)(cos(angle) * 12.5f);
        int y = cy + (int)(sin(angle) * 5.0f);
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            screen[y][x] = '#';
    }
    
    // Start/Finish line
    for (int i = -4; i <= 4; i++) {
        int x = cx + i;
        int y = cy - 8;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            screen[y][x] = (i % 2 == 0) ? 'H' : 'H';
    }
}

void drawCar() {
    int x = (int)(player.x + 0.5f);
    int y = (int)(player.y + 0.5f);
    
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        float a = fmod(player.angle + M_PI * 2.5f, M_PI * 2);
        char c = (a < M_PI/4 || a >= 7*M_PI/4) ? '>' : 
                 (a < 3*M_PI/4) ? '^' : 
                 (a < 5*M_PI/4) ? '<' : 'v';
        screen[y][x] = c;
    }
}

void render() {
    clearScreen();
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            char c = screen[y][x];
            if (c == '#') printf(CLR_GRAY "%c" CLR_RESET, c);
            else if (c == ',' || c == ' ') printf(CLR_GREEN "%c" CLR_RESET, c);
            else if (c == '.' || c == ':') printf(CLR_BROWN "%c" CLR_RESET, c);
            else if (c == 'H') printf(CLR_YELLOW "%c" CLR_RESET, c);
            else if (c == '>' || c == '<' || c == '^' || c == 'v') 
                printf(CLR_RED "%c" CLR_RESET, c);
            else printf("%c", c);
        }
        printf("\n");
    }
    
    float speed = sqrt(player.vx * player.vx + player.vy * player.vy);
    float elapsed = (float)(clock() - startTime) / CLOCKS_PER_SEC;
    
    printf(CLR_CYAN "╔════════════════════════════════════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_YELLOW " Lap: %d/%d " CLR_RESET, player.lap, totalLaps);
    printf(CLR_GREEN "│ Speed: %3.0f " CLR_RESET, speed * 10);
    printf("│ Time: %.1fs " CLR_RESET, elapsed);
    printf("│ WASD/Arrows:Move Q:Quit\n");
}

void updatePhysics() {
    player.vx *= 0.94f;
    player.vy *= 0.94f;
    
    player.x += player.vx;
    player.y += player.vy;
    
    if (!isOnTrack((int)player.x, (int)player.y)) {
        player.vx *= 0.6f;
        player.vy *= 0.6f;
        float cx = WIDTH / 2.0f;
        float cy = HEIGHT / 2.0f;
        float dx = cx - player.x;
        float dy = cy - player.y;
        float d = sqrt(dx*dx + dy*dy);
        if (d > 0) {
            player.x += (dx/d) * 0.3f;
            player.y += (dy/d) * 0.3f;
        }
    }
    
    if (player.x < 0) player.x = 0;
    if (player.x >= WIDTH) player.x = WIDTH - 1;
    if (player.y < 0) player.y = 0;
    if (player.y >= HEIGHT) player.y = HEIGHT - 1;
}

void checkLap() {
    float dx = player.x - WIDTH/2;
    float dy = player.y - HEIGHT/2 + 8;
    float angle = atan2(player.y - HEIGHT/2, (player.x - WIDTH/2)/2.5f);
    int cp = (int)((angle + M_PI) / (M_PI/8)) % 16;
    
    if (fabs(dx) < 5 && fabs(dy) < 1) {
        if (player.vy < -0.1f && player.checkpoint >= 8) {
            player.lap++;
            player.checkpoint = 0;
            if (player.lap >= totalLaps) gameRunning = 0;
        }
    } else if (cp != player.checkpoint && abs(cp - player.checkpoint) < 3) {
        player.checkpoint = cp;
    }
}

void handleInput() {
    if (kbhit()) {
        int ch = getch();
        if (ch == 27) { getch(); ch = getch(); }
        
        float accel = 0.2f;
        float turn = 0.12f;
        
        switch(ch) {
            case 'w': case 'W': case 65:
                player.vx += cos(player.angle) * accel;
                player.vy += sin(player.angle) * accel;
                break;
            case 's': case 'S': case 66:
                player.vx *= 0.85f;
                player.vy *= 0.85f;
                break;
            case 'a': case 'A': case 68:
                player.angle -= turn;
                break;
            case 'd': case 'D': case 67:
                player.angle += turn;
                break;
            case 'q': case 'Q':
                gameRunning = 0;
                break;
        }
    }
}

int main() {
    enableRawMode();
    clearScreen();
    
    printf(CLR_YELLOW);
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║           🏁  SUPER OFF ROAD RACER  🏁                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf(CLR_RESET);
    printf("\nRace %d laps around the track!\n", totalLaps);
    printf("Stay on the dirt (avoid grass) for maximum speed.\n\n");
    printf(CLR_GREEN "Controls:\n" CLR_RESET);
    printf("  W / ↑  - Accelerate\n");
    printf("  S / ↓  - Brake\n");
    printf("  A / ←  - Turn Left\n");
    printf("  D / →  - Turn Right\n");
    printf("  Q      - Quit\n\n");
    printf("Press any key to start...");
    fflush(stdout);
    
    getch();
    
    initCar();
    startTime = clock();
    
    while (gameRunning) {
        handleInput();
        updatePhysics();
        checkLap();
        drawTrack();
        drawCar();
        render();
        SLEEP(33);
    }
    
    clearScreen();
    float finalTime = (float)(clock() - startTime) / CLOCKS_PER_SEC;
    
    printf(CLR_YELLOW "\n\n");
    printf("╔═══════════════════════════════════════╗\n");
    printf("║      🏆  RACE COMPLETE!  🏆          ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    printf(CLR_RESET);
    printf("Laps completed: %d\n", player.lap);
    printf("Total time: %.2f seconds\n\n", finalTime);
    printf("Thanks for playing!\n\n");
    
    disableRawMode();
    return 0;
}
