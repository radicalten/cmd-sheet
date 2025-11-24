#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR "cls"
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #define CLEAR "clear"
#endif

#define WIDTH 40
#define HEIGHT 20
#define ROAD_LEFT 12
#define ROAD_RIGHT 28
#define PLAYER_START_X 20
#define PLAYER_START_Y 16

typedef struct {
    int x, y;
    int active;
} Car;

char screen[HEIGHT][WIDTH];
int playerX = PLAYER_START_X;
int score = 0;
int gameOver = 0;
Car obstacles[5];
int roadOffset = 0;

// Terminal handling for non-blocking input
#ifndef _WIN32
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
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
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int getch() {
    int ch = getchar();
    return ch;
}
#endif

void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void initScreen() {
    for(int i = 0; i < HEIGHT; i++) {
        for(int j = 0; j < WIDTH; j++) {
            screen[i][j] = ' ';
        }
    }
}

void drawRoad() {
    // Draw grass
    for(int i = 0; i < HEIGHT; i++) {
        for(int j = 0; j < WIDTH; j++) {
            screen[i][j] = '.';
        }
    }
    
    // Draw road
    for(int i = 0; i < HEIGHT; i++) {
        for(int j = ROAD_LEFT; j <= ROAD_RIGHT; j++) {
            screen[i][j] = ' ';
        }
        // Road edges
        screen[i][ROAD_LEFT] = '|';
        screen[i][ROAD_RIGHT] = '|';
    }
    
    // Draw road markings
    for(int i = 0; i < HEIGHT; i++) {
        if((i + roadOffset) % 4 == 0) {
            screen[i][WIDTH/2] = ':';
        }
    }
}

void initObstacles() {
    for(int i = 0; i < 5; i++) {
        obstacles[i].active = 0;
    }
}

void spawnObstacle() {
    for(int i = 0; i < 5; i++) {
        if(!obstacles[i].active) {
            obstacles[i].x = ROAD_LEFT + 2 + (rand() % (ROAD_RIGHT - ROAD_LEFT - 4));
            obstacles[i].y = 0;
            obstacles[i].active = 1;
            break;
        }
    }
}

void updateObstacles() {
    for(int i = 0; i < 5; i++) {
        if(obstacles[i].active) {
            obstacles[i].y++;
            if(obstacles[i].y >= HEIGHT) {
                obstacles[i].active = 0;
                score += 10;
            }
        }
    }
}

void drawObstacles() {
    for(int i = 0; i < 5; i++) {
        if(obstacles[i].active && obstacles[i].y >= 0 && obstacles[i].y < HEIGHT) {
            screen[obstacles[i].y][obstacles[i].x] = '#';
            if(obstacles[i].x + 1 < WIDTH) screen[obstacles[i].y][obstacles[i].x + 1] = '#';
        }
    }
}

void drawPlayer() {
    if(PLAYER_START_Y < HEIGHT && playerX >= 0 && playerX < WIDTH) {
        screen[PLAYER_START_Y][playerX] = 'A';
        if(playerX + 1 < WIDTH) screen[PLAYER_START_Y][playerX + 1] = 'A';
        if(PLAYER_START_Y - 1 >= 0) {
            screen[PLAYER_START_Y - 1][playerX] = '|';
            if(playerX + 1 < WIDTH) screen[PLAYER_START_Y - 1][playerX + 1] = '|';
        }
    }
}

int checkCollision() {
    // Check wall collision
    if(playerX <= ROAD_LEFT || playerX + 1 >= ROAD_RIGHT) {
        return 1;
    }
    
    // Check obstacle collision
    for(int i = 0; i < 5; i++) {
        if(obstacles[i].active) {
            if(obstacles[i].y >= PLAYER_START_Y - 1 && obstacles[i].y <= PLAYER_START_Y) {
                if(abs(obstacles[i].x - playerX) <= 2) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

void displayScreen() {
    system(CLEAR);
    
    printf("\n  ╔════════════════════════════════════════╗\n");
    printf("  ║        TERMINAL RACING GAME            ║\n");
    printf("  ╚════════════════════════════════════════╝\n\n");
    
    for(int i = 0; i < HEIGHT; i++) {
        printf("  ");
        for(int j = 0; j < WIDTH; j++) {
            char c = screen[i][j];
            if(c == 'A' || c == '|') {
                printf("\033[1;32m%c\033[0m", c); // Green player
            } else if(c == '#') {
                printf("\033[1;31m%c\033[0m", c); // Red obstacles
            } else if(c == '.') {
                printf("\033[0;32m%c\033[0m", c); // Green grass
            } else if(c == '|') {
                printf("\033[1;33m%c\033[0m", c); // Yellow road edge
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    
    printf("\n  Score: %d\n", score);
    printf("  Controls: A/D or Left/Right arrows to move, Q to quit\n");
}

int main() {
    srand(time(NULL));
    
#ifndef _WIN32
    enableRawMode();
#endif

    initObstacles();
    int frameCount = 0;
    
    printf("Press any key to start...\n");
    getch();
    
    while(!gameOver) {
        initScreen();
        drawRoad();
        drawObstacles();
        drawPlayer();
        displayScreen();
        
        // Handle input
        if(kbhit()) {
            char c = getch();
            if(c == 'a' || c == 'A' || c == 68 || c == 75) { // Left
                playerX -= 2;
            } else if(c == 'd' || c == 'D' || c == 67 || c == 77) { // Right
                playerX += 2;
            } else if(c == 'q' || c == 'Q') {
                break;
            }
            
            // Arrow key handling (reads extra characters)
            if(c == 27) { // ESC sequence
                getch(); // skip [
                c = getch();
                if(c == 67) playerX += 2; // Right arrow
                if(c == 68) playerX -= 2; // Left arrow
            }
        }
        
        // Spawn obstacles
        if(frameCount % 20 == 0) {
            spawnObstacle();
        }
        
        // Update game
        updateObstacles();
        roadOffset++;
        frameCount++;
        
        // Check collision
        if(checkCollision()) {
            gameOver = 1;
        }
        
        sleepMs(100);
    }
    
    // Game over screen
    system(CLEAR);
    printf("\n\n");
    printf("  ╔════════════════════════════════════════╗\n");
    printf("  ║            GAME OVER!                  ║\n");
    printf("  ║                                        ║\n");
    printf("  ║         Final Score: %-6d           ║\n", score);
    printf("  ║                                        ║\n");
    printf("  ╚════════════════════════════════════════╝\n\n");
    
#ifndef _WIN32
    disableRawMode();
#endif
    
    return 0;
}
