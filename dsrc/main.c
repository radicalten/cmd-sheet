/*
 * ASCII OUTRUN - A Single-File C Racing Game
 * No external dependencies. Uses Standard C + OS headers.
 *
 * COMPILE:
 *   Linux/macOS: gcc racer.c -o racer -lm
 *   Windows:     gcc racer.c -o racer
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* --- CROSS-PLATFORM UTILS --- */
#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    void sleep_ms(int milliseconds) { Sleep(milliseconds); }
    void setup_terminal() {
        // Enable ANSI escape codes on Windows 10+
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, dwMode);
    }
    void restore_terminal() {}
    int key_hit() { return _kbhit(); }
    int read_key() { return _getch(); }
#else
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>

    void sleep_ms(int milliseconds) { usleep(milliseconds * 1000); }

    struct termios orig_termios;
    void restore_terminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        printf("\x1b[?25h"); // Show cursor
    }

    void setup_terminal() {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(restore_terminal);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    int key_hit() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return select(1, &fds, NULL, NULL, &tv) > 0;
    }

    int read_key() {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) return c;
        return 0;
    }
#endif

/* --- GAME CONSTANTS --- */
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define ROAD_WIDTH 2000.0f
#define SEGMENT_LENGTH 0.2f
#define DRAW_DISTANCE 300

/* --- MATH UTILS --- */
typedef struct {
    float x, y, z; // World coordinates
    float X, Y, W; // Screen coordinates
    float curve;   // Curvature of this segment
    float spriteX; // X position of obstacle (-1 to 1)
    int spriteType; // 0=none, 1=tree, 2=rock
} Segment;

Segment segments[10000]; // The track
int segment_count = 0;

void project(Segment *p, float cameraX, float cameraY, float cameraZ, float camDepth, float width, float height, float roadWidth) {
    // Translate to camera coords
    p->X = p->x - cameraX;
    p->Y = p->y - cameraY;
    float z = p->z - cameraZ;
    
    // Handle looping map (not strictly necessary for linear generation but good for safety)
    if (z < 0) z = 1; 
    
    p->W = 1.0f / z; // Inverse Z (Scaling factor)
    
    // Project to screen coords
    // Screen X = CenterX + (Scale * DeltaX * RoadWidth)
    p->X = width / 2 + (camDepth * p->X * width / 2) * p->W;
    p->Y = height / 2 - (camDepth * p->Y * height / 2) * p->W;
    p->W = p->W * roadWidth * width / 2; // Projected Width
}

void add_segment(float curve) {
    int n = segment_count;
    segments[n].x = 0;
    segments[n].y = 0; // Flat road for now
    segments[n].z = (n + 1) * SEGMENT_LENGTH;
    segments[n].curve = curve;
    
    // Random obstacles
    if (rand() % 100 < 5) {
        segments[n].spriteType = 1 + (rand() % 2); // 1 or 2
        segments[n].spriteX = ((rand() % 100) / 50.0f) - 1.0f; // -1 to 1
    } else {
        segments[n].spriteType = 0;
    }
    
    segment_count++;
}

/* --- DRAWING PRIMITIVES --- */
// Simple buffer to avoid flickering
char screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH + 1];

void clear_buffer() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen_buffer[y][x] = ' ';
        }
        screen_buffer[y][SCREEN_WIDTH] = '\0';
    }
}

void draw_scanline(int y, int x1, int x2, char c) {
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= SCREEN_WIDTH) x2 = SCREEN_WIDTH - 1;
    
    for (int x = x1; x <= x2; x++) {
        screen_buffer[y][x] = c;
    }
}

void draw_text(int x, int y, const char* text) {
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        if (x + i >= SCREEN_WIDTH) break;
        screen_buffer[y][x + i] = text[i];
    }
}

/* --- MAIN GAME --- */
int main() {
    setup_terminal();
    srand(time(NULL));

    // Hide Cursor
    printf("\x1b[?25l");

    // Generate Track
    // 0 = Straight, positive = right, negative = left
    for (int i = 0; i < 500; i++) add_segment(0);
    for (int i = 0; i < 1000; i++) add_segment(0.5f);
    for (int i = 0; i < 1000; i++) add_segment(-0.5f);
    for (int i = 0; i < 500; i++) add_segment(1.2f);
    for (int i = 0; i < 500; i++) add_segment(-1.2f);
    for (int i = 0; i < 2000; i++) add_segment(0);

    float playerX = 0;
    float playerZ = 0;
    float speed = 0;
    float maxSpeed = SEGMENT_LENGTH * 600.0f; // Approx speed unit
    float accel = maxSpeed / 5.0f;
    float breaking = -maxSpeed;
    float decel = -maxSpeed / 5.0f;
    float offRoadDecel = -maxSpeed / 2.0f;
    float offRoadLimit = maxSpeed / 4.0f;

    double score = 0;

    while (1) {
        // 1. Input
        if (key_hit()) {
            char k = read_key();
            if (k == 'w' || k == 'W') speed += accel * 0.033f;
            if (k == 's' || k == 'S') speed += breaking * 0.033f;
            if (k == 'a' || k == 'A') playerX -= 1.5f * 0.033f;
            if (k == 'd' || k == 'D') playerX += 1.5f * 0.033f;
            if (k == 'q' || k == 'Q') break; // Quit
        } else {
            speed += decel * 0.033f;
        }

        // Physics Caps
        if (speed > maxSpeed) speed = maxSpeed;
        if (speed < 0) speed = 0;

        // Move Car
        playerZ += speed * 0.033f;
        
        // Handle Centrifugal Force (simple approximation)
        // Find current segment to determine curve
        int current_index = (int)(playerZ / SEGMENT_LENGTH) % segment_count;
        float current_curve = segments[current_index].curve;
        
        // If moving, curve pushes player
        playerX -= (current_curve * (speed/maxSpeed) * speed/maxSpeed) * 0.05f;

        // Off-road slowdown
        if ((playerX < -1.0f || playerX > 1.0f) && speed > offRoadLimit)
            speed += offRoadDecel * 0.033f;

        // Clamp PlayerX
        if (playerX < -2.0f) playerX = -2.0f;
        if (playerX > 2.0f) playerX = 2.0f;

        score += speed * 0.033f;

        // 2. Render Preparation
        clear_buffer();

        // Draw Sky (Simple Gradient)
        for(int y=0; y<SCREEN_HEIGHT/2; y++) {
            draw_scanline(y, 0, SCREEN_WIDTH, '.');
        }
        // Draw Horizon
        draw_scanline(SCREEN_HEIGHT/2 - 1, 0, SCREEN_WIDTH, '#');

        // Project and Draw Road
        float maxy = SCREEN_HEIGHT;
        float camDepth = 0.84f; // Field of View factor

        int startPos = (int)(playerZ / SEGMENT_LENGTH);
        float x = 0, dx = 0;
        float camHeight = 1500 + segments[startPos].y;

        for (int n = startPos; n < startPos + DRAW_DISTANCE; n++) {
            Segment *line = &segments[n % segment_count];
            
            // Loop logic for infinite track feel (offset Z)
            line->x = 0; 
            float loopZ = line->z;
            if (n >= segment_count) loopZ += segment_count * SEGMENT_LENGTH;

            // Project
            project(line, (playerX * ROAD_WIDTH), camHeight, playerZ - (n >= segment_count ? segment_count * SEGMENT_LENGTH : 0), camDepth, SCREEN_WIDTH, SCREEN_HEIGHT, ROAD_WIDTH);

            // Accumulate curvature for x-offset (simulates road bending)
            x += dx;
            dx += line->curve;
            
            // Correct projected X by the accumulated curve
            line->X += x;

            // Clip: If this segment is below the previous one (hidden), skip
            // Or if it's above the screen
            if (line->Y >= maxy) continue;
            if (line->Y < SCREEN_HEIGHT/2) continue; // Clip at horizon

            Segment *prev = &segments[(n - 1) % segment_count];
            // Note: Using previous projected values directly is tricky with the loop logic above, 
            // so we usually just draw bands based on current line height to bottom of previous.
            // For simple ASCII, scanline from current Y to MaxY is easiest.

            float prevY = (n == startPos) ? SCREEN_HEIGHT : segments[(n-1)%segment_count].Y;
            if (prevY < line->Y) prevY = line->Y + 1; // Safety

            // Colors based on alternate segments
            char grassChar = ((n / 3) % 2) ? ' ' : '.'; // Light/Dark Grass
            char rumbleChar = ((n / 3) % 2) ? 'H' : '='; // Rumble strip
            char roadChar = ((n / 3) % 2) ? ' ' : '`'; // Road texture

            // Draw Row
            int Y = (int)line->Y;
            int pY = (int)maxy;
            
            // Draw strictly from current line up to the last drawn line (painters algo reverse)
            // Actually, for "Outrun" style, we usually draw Back-to-Front or Front-to-Back.
            // Front-to-back (what we are doing) requires tracking maxy.
            
            if (Y < pY) {
                draw_scanline(Y, 0, SCREEN_WIDTH, grassChar); // Grass
                draw_scanline(Y, (int)(line->X - line->W * 1.2), (int)(line->X + line->W * 1.2), rumbleChar); // Rumble
                draw_scanline(Y, (int)(line->X - line->W), (int)(line->X + line->W), roadChar); // Road
                
                // Lane marker
                if ((n / 10) % 2) {
                    draw_scanline(Y, (int)(line->X - line->W * 0.05), (int)(line->X + line->W * 0.05), '|');
                }
            }
            maxy = Y;
        }

        // Draw Car (Simple ASCII Art)
        int carY = SCREEN_HEIGHT - 4;
        int carX = SCREEN_WIDTH / 2 - 3;
        draw_text(carX, carY,     " [##] ");
        draw_text(carX, carY + 1, "//  \\\\");
        draw_text(carX, carY + 2, "[_||_]");

        // UI
        char hud[64];
        sprintf(hud, "SPEED: %d mph  SCORE: %d", (int)(speed/10), (int)score);
        draw_text(2, 1, hud);
        draw_text(2, 2, "CONTROLS: W,A,S,D, Q=Quit");

        // 3. Print Buffer
        printf("\x1b[H"); // Go to Home (0,0)
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            printf("%s\n", screen_buffer[y]);
        }

        // FPS Cap
        sleep_ms(33); // ~30 FPS
    }

    // Cleanup
    printf("\x1b[?25h"); // Show cursor
    restore_terminal();
    return 0;
}
