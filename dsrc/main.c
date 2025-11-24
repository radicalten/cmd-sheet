#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#define WIDTH 80
#define HEIGHT 24
#define ROAD_SEGMENTS 100
#define DRAW_DISTANCE 50

typedef struct {
    float curve;
    float y;
} Segment;

char screen[HEIGHT][WIDTH];
Segment road[ROAD_SEGMENTS];
float playerX = 0;
float position = 0;
float speed = 0;
int score = 0;

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

void initTerminal() {
#ifndef _WIN32
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &term);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
}

void resetTerminal() {
#ifndef _WIN32
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(0, TCSANOW, &term);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);
#endif
}

int getKey() {
#ifdef _WIN32
    if (_kbhit()) return _getch();
    return 0;
#else
    int ch = getchar();
    return (ch == EOF) ? 0 : ch;
#endif
}

void initRoad() {
    for (int i = 0; i < ROAD_SEGMENTS; i++) {
        road[i].y = 0;
        road[i].curve = 0;
        
        if (i > 0) {
            road[i].y = road[i-1].y + 100;
            
            if (i % 20 == 0) {
                road[i].curve = (rand() % 3 - 1) * 2.0f;
            } else {
                road[i].curve = road[i-1].curve;
            }
        }
    }
}

void drawScreen() {
    memset(screen, ' ', sizeof(screen));
    
    int startPos = (int)position / 100;
    float camX = playerX;
    
    for (int n = 0; n < DRAW_DISTANCE; n++) {
        int segIdx = (startPos + n) % ROAD_SEGMENTS;
        Segment *seg = &road[segIdx];
        
        float perspective = (float)(n + 1) / DRAW_DISTANCE;
        float roadY = HEIGHT - 1 - (int)(perspective * HEIGHT * 0.5f);
        
        if (roadY < 0 || roadY >= HEIGHT) continue;
        
        float roadWidth = 0.1f + perspective * 0.8f;
        float clipWidth = roadWidth * 0.15f;
        
        float curvature = 0;
        for (int i = startPos; i < startPos + n; i++) {
            curvature += road[i % ROAD_SEGMENTS].curve * 0.02f;
        }
        
        float roadCenter = WIDTH / 2.0f + curvature * (1.0f - perspective) * WIDTH * 0.3f;
        float leftGrass = roadCenter - roadWidth * WIDTH / 2.0f;
        float leftEdge = roadCenter - roadWidth * WIDTH / 2.0f + clipWidth * WIDTH;
        float rightEdge = roadCenter + roadWidth * WIDTH / 2.0f - clipWidth * WIDTH;
        float rightGrass = roadCenter + roadWidth * WIDTH / 2.0f;
        
        int y = (int)roadY;
        if (y >= 0 && y < HEIGHT) {
            for (int x = 0; x < WIDTH; x++) {
                char c = ' ';
                
                if (x < leftGrass || x > rightGrass) {
                    c = (n % 3 == 0) ? ',' : '.';
                } else if (x < leftEdge || x > rightEdge) {
                    c = (n % 2 == 0) ? '#' : '|';
                } else {
                    int stripe = ((int)(position + n * 10)) % 40;
                    if (x > roadCenter - 2 && x < roadCenter + 2) {
                        c = (stripe < 10) ? '|' : ' ';
                    } else {
                        c = (n % 2 == 0) ? 177 : 176;
                    }
                }
                
                screen[y][x] = c;
            }
        }
    }
    
    int carY = HEIGHT - 4;
    int carX = WIDTH / 2 + (int)(playerX * WIDTH / 4);
    
    if (carX >= 1 && carX < WIDTH - 2 && carY >= 2 && carY < HEIGHT - 1) {
        screen[carY - 2][carX] = 'o';
        screen[carY - 1][carX - 1] = '/';
        screen[carY - 1][carX] = '|';
        screen[carY - 1][carX + 1] = '\\';
        screen[carY][carX - 1] = '=';
        screen[carY][carX] = 'H';
        screen[carY][carX + 1] = '=';
        screen[carY + 1][carX - 1] = 'O';
        screen[carY + 1][carX + 1] = 'O';
    }
    
    clearScreen();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            putchar(screen[y][x]);
        }
        putchar('\n');
    }
    
    printf("Speed: %3.0f  Score: %d  Pos: %.0f\n", speed * 100, score, position);
    printf("Controls: A/D or Arrow Keys to steer, W/S for speed, Q to quit\n");
}

int main() {
    srand(time(NULL));
    initTerminal();
    initRoad();
    
    speed = 0.5f;
    int running = 1;
    
    clock_t lastTime = clock();
    
    while (running) {
        clock_t currentTime = clock();
        float deltaTime = (float)(currentTime - lastTime) / CLOCKS_PER_SEC;
        lastTime = currentTime;
        
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        
        int key = getKey();
        if (key == 'q' || key == 'Q' || key == 27) {
            running = 0;
        }
        if (key == 'a' || key == 'A' || key == 68) {
            playerX -= 1.5f * deltaTime;
        }
        if (key == 'd' || key == 'D' || key == 67) {
            playerX += 1.5f * deltaTime;
        }
        if (key == 'w' || key == 'W') {
            speed += 0.5f * deltaTime;
            if (speed > 1.5f) speed = 1.5f;
        }
        if (key == 's' || key == 'S') {
            speed -= 0.5f * deltaTime;
            if (speed < 0.2f) speed = 0.2f;
        }
        
        position += speed * 1000 * deltaTime;
        score = (int)position / 10;
        
        if (playerX < -1.0f) playerX = -1.0f;
        if (playerX > 1.0f) playerX = 1.0f;
        
        int segIdx = ((int)position / 100) % ROAD_SEGMENTS;
        playerX += road[segIdx].curve * 0.001f * speed;
        
        drawScreen();
        
#ifdef _WIN32
        Sleep(33);
#else
        usleep(33000);
#endif
    }
    
    resetTerminal();
    printf("\n\nGame Over! Final Score: %d\n", score);
    
    return 0;
}
