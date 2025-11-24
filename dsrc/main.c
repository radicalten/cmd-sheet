#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#define WIDTH 70
#define HEIGHT 28
#define TOTAL_LAPS 3
#define NUM_ENEMIES 3

typedef struct {
    float x, y;
    float angle;
    float speed;
    int lap;
    int checkpoint;
} Car;

char screen[HEIGHT][WIDTH];
Car player;
Car enemies[NUM_ENEMIES];
time_t start_time;
int game_over = 0;
int won = 0;

#ifdef _WIN32
void sleep_ms(int ms) { Sleep(ms); }
int kbhit() { return _kbhit(); }
char get_input() { return _getch(); }
#else
void sleep_ms(int ms) { usleep(ms * 1000); }
int kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}
char get_input() { return getchar(); }
#endif

void clear_screen() { printf("\033[2J\033[H"); }

void draw_track() {
    for (int i = 0; i < HEIGHT; i++)
        for (int j = 0; j < WIDTH; j++)
            screen[i][j] = ' ';
    
    // Figure-8 track with two circles connected
    float cx1 = 20, cy1 = 14;  // Left circle center
    float cx2 = 50, cy2 = 14;  // Right circle center
    float r_outer = 10, r_inner = 7;
    
    // Draw left circle
    for (float a = 0; a < 2 * M_PI; a += 0.02) {
        int x1 = (int)(cx1 + cos(a) * r_outer);
        int y1 = (int)(cy1 + sin(a) * r_outer);
        int x2 = (int)(cx1 + cos(a) * r_inner);
        int y2 = (int)(cy1 + sin(a) * r_inner);
        if (x1 >= 0 && x1 < WIDTH && y1 >= 0 && y1 < HEIGHT) screen[y1][x1] = '#';
        if (x2 >= 0 && x2 < WIDTH && y2 >= 0 && y2 < HEIGHT) screen[y2][x2] = '#';
    }
    
    // Draw right circle
    for (float a = 0; a < 2 * M_PI; a += 0.02) {
        int x1 = (int)(cx2 + cos(a) * r_outer);
        int y1 = (int)(cy2 + sin(a) * r_outer);
        int x2 = (int)(cx2 + cos(a) * r_inner);
        int y2 = (int)(cy2 + sin(a) * r_inner);
        if (x1 >= 0 && x1 < WIDTH && y1 >= 0 && y1 < HEIGHT) screen[y1][x1] = '#';
        if (x2 >= 0 && x2 < WIDTH && y2 >= 0 && y2 < HEIGHT) screen[y2][x2] = '#';
    }
    
    // Draw start/finish line
    for (int i = 0; i < 4; i++) {
        int x = 20 + r_inner;
        int y = 14 - 2 + i;
        if (y >= 0 && y < HEIGHT && x < WIDTH) screen[y][x] = '|';
    }
}

int is_on_track(float x, float y) {
    int ix = (int)(x + 0.5);
    int iy = (int)(y + 0.5);
    if (ix <= 0 || ix >= WIDTH-1 || iy <= 0 || iy >= HEIGHT-1) return 0;
    
    // Check if we're hitting a wall
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy;
            if (cx >= 0 && cx < WIDTH && cy >= 0 && cy < HEIGHT)
                if (screen[cy][cx] == '#') return 0;
        }
    return 1;
}

void update_checkpoint(Car *car) {
    // Checkpoints to ensure lap is counted correctly
    if (car->checkpoint == 0 && car->x > 35 && car->x < 45) {
        car->checkpoint = 1;
    } else if (car->checkpoint == 1 && car->x > 45) {
        car->checkpoint = 2;
    } else if (car->checkpoint == 2 && car->x < 35) {
        car->checkpoint = 3;
    } else if (car->checkpoint == 3 && car->x > 26 && car->x < 29 && 
               car->y > 12 && car->y < 16) {
        car->lap++;
        car->checkpoint = 0;
    }
}

void init_game() {
    player.x = 30; player.y = 14;
    player.angle = 0; player.speed = 0;
    player.lap = 0; player.checkpoint = 0;
    
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i].x = 30 - (i + 1) * 4;
        enemies[i].y = 14;
        enemies[i].angle = 0;
        enemies[i].speed = 0.25 + i * 0.08;
        enemies[i].lap = 0;
        enemies[i].checkpoint = 0;
    }
    
    start_time = time(NULL);
    game_over = 0; won = 0;
}

void update_player(char input) {
    if (input == 'w' || input == 'W') {
        player.speed += 0.08;
        if (player.speed > 0.8) player.speed = 0.8;
    }
    if (input == 's' || input == 'S') {
        player.speed -= 0.08;
        if (player.speed < -0.4) player.speed = -0.4;
    }
    if (input == 'a' || input == 'A') player.angle -= 0.12;
    if (input == 'd' || input == 'D') player.angle += 0.12;
    
    player.speed *= 0.96;
    
    float new_x = player.x + cos(player.angle) * player.speed;
    float new_y = player.y + sin(player.angle) * player.speed;
    
    if (is_on_track(new_x, new_y)) {
        player.x = new_x;
        player.y = new_y;
    } else {
        player.speed *= -0.4;
    }
    
    update_checkpoint(&player);
    if (player.lap >= TOTAL_LAPS) { game_over = 1; won = 1; }
}

void update_enemies() {
    for (int i = 0; i < NUM_ENEMIES; i++) {
        Car *e = &enemies[i];
        float tx, ty;
        
        if (e->checkpoint <= 1) { tx = 50; ty = 14; }
        else { tx = 30; ty = 14; }
        
        float target_angle = atan2(ty - e->y, tx - e->x);
        float diff = target_angle - e->angle;
        while (diff > M_PI) diff -= 2 * M_PI;
        while (diff < -M_PI) diff += 2 * M_PI;
        e->angle += diff * 0.08;
        
        float new_x = e->x + cos(e->angle) * e->speed;
        float new_y = e->y + sin(e->angle) * e->speed;
        
        if (is_on_track(new_x, new_y)) {
            e->x = new_x;
            e->y = new_y;
        } else {
            e->angle += 0.5;
        }
        
        update_checkpoint(e);
    }
}

void render() {
    clear_screen();
    draw_track();
    
    for (int i = 0; i < NUM_ENEMIES; i++) {
        int ex = (int)(enemies[i].x + 0.5);
        int ey = (int)(enemies[i].y + 0.5);
        if (ex > 0 && ex < WIDTH-1 && ey > 0 && ey < HEIGHT-1)
            screen[ey][ex] = 'E';
    }
    
    int px = (int)(player.x + 0.5);
    int py = (int)(player.y + 0.5);
    if (px > 0 && px < WIDTH-1 && py > 0 && py < HEIGHT-1)
        screen[py][px] = 'P';
    
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            char c = screen[i][j];
            if (c == '#') printf("\033[32m#\033[0m");
            else if (c == 'P') printf("\033[33mP\033[0m");
            else if (c == 'E') printf("\033[31mE\033[0m");
            else if (c == '|') printf("\033[37m|\033[0m");
            else printf(" ");
        }
        printf("\n");
    }
    
    time_t now = time(NULL);
    int elapsed = (int)(now - start_time);
    printf("\n\033[36mLap: %d/%d | Time: %02d:%02d | Speed: %.1f | [WASD]=Move [Q]=Quit\033[0m\n",
           player.lap, TOTAL_LAPS, elapsed / 60, elapsed % 60, player.speed * 10);
}

void show_victory() {
    clear_screen();
    int elapsed = (int)(time(NULL) - start_time);
    printf("\n\n\n");
    printf("          \033[33m╔════════════════════════════════╗\033[0m\n");
    printf("          \033[33m║                                ║\033[0m\n");
    printf("          \033[33m║       🏁  VICTORY!  🏁        ║\033[0m\n");
    printf("          \033[33m║                                ║\033[0m\n");
    printf("          \033[33m║   You completed %d laps!       ║\033[0m\n", TOTAL_LAPS);
    printf("          \033[33m║                                ║\033[0m\n");
    printf("          \033[33m║   Final Time: %02d:%02d           ║\033[0m\n", elapsed/60, elapsed%60);
    printf("          \033[33m║                                ║\033[0m\n");
    printf("          \033[33m╚════════════════════════════════╝\033[0m\n");
    printf("\n\n");
}

int main() {
    printf("\033[36m=== FIGURE-8 RACING ===\033[0m\n");
    printf("Complete %d laps around the figure-8 track!\n", TOTAL_LAPS);
    printf("Controls: W=Forward, S=Brake, A=Left, D=Right, Q=Quit\n");
    printf("Press ENTER to start...\n");
    getchar();
    
    #ifndef _WIN32
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    #endif
    
    init_game();
    
    while (!game_over) {
        char input = 0;
        if (kbhit()) {
            input = get_input();
            if (input == 'q' || input == 'Q') break;
        }
        
        update_player(input);
        update_enemies();
        render();
        sleep_ms(40);
    }
    
    #ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    #endif
    
    if (won) show_victory();
    printf("\n\033[36mThanks for playing!\033[0m\n\n");
    
    return 0;
}
