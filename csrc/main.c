#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define CLEAR "cls"
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#define CLEAR "clear"
#endif

#define WIDTH 40
#define HEIGHT 20
#define MAX_BULLETS 20
#define MAX_ENEMIES 24
#define MAX_ENEMY_BULLETS 10

typedef struct { int x, y, active; } Bullet;
typedef struct { int x, y, active, type; } Enemy;
typedef struct { int x, y, lives; } Player;

Player player;
Bullet bullets[MAX_BULLETS];
Bullet enemy_bullets[MAX_ENEMY_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score = 0;
int game_over = 0;
int frame = 0;

#ifndef _WIN32
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
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

int getch() {
    int c = getchar();
    return c;
}
#endif

void init_game() {
    player.x = WIDTH / 2;
    player.y = HEIGHT - 2;
    player.lives = 3;
    
    memset(bullets, 0, sizeof(bullets));
    memset(enemy_bullets, 0, sizeof(enemy_bullets));
    
    int idx = 0;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 6; col++) {
            if (idx < MAX_ENEMIES) {
                enemies[idx].x = col * 6 + 5;
                enemies[idx].y = row * 2 + 2;
                enemies[idx].active = 1;
                enemies[idx].type = row % 3;
                idx++;
            }
        }
    }
}

void shoot_bullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = player.x;
            bullets[i].y = player.y - 1;
            bullets[i].active = 1;
            break;
        }
    }
}

void enemy_shoot() {
    if (rand() % 30 != 0) return;
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && rand() % 8 == 0) {
            for (int j = 0; j < MAX_ENEMY_BULLETS; j++) {
                if (!enemy_bullets[j].active) {
                    enemy_bullets[j].x = enemies[i].x;
                    enemy_bullets[j].y = enemies[i].y + 1;
                    enemy_bullets[j].active = 1;
                    return;
                }
            }
        }
    }
}

void update_bullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].y--;
            if (bullets[i].y < 1) bullets[i].active = 0;
        }
    }
    
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemy_bullets[i].active) {
            enemy_bullets[i].y++;
            if (enemy_bullets[i].y >= HEIGHT - 1) enemy_bullets[i].active = 0;
        }
    }
}

void update_enemies() {
    static int direction = 1;
    
    if (frame % 20 != 0) return;
    
    int should_reverse = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            if ((enemies[i].x <= 1 && direction < 0) || 
                (enemies[i].x >= WIDTH - 2 && direction > 0)) {
                should_reverse = 1;
                break;
            }
        }
    }
    
    if (should_reverse) {
        direction = -direction;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) enemies[i].y++;
        }
    } else {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) enemies[i].x += direction;
        }
    }
}

void check_collisions() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active && 
                    abs(bullets[i].x - enemies[j].x) <= 1 && 
                    bullets[i].y == enemies[j].y) {
                    bullets[i].active = 0;
                    enemies[j].active = 0;
                    score += (enemies[j].type + 1) * 10;
                }
            }
        }
    }
    
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemy_bullets[i].active && 
            abs(enemy_bullets[i].x - player.x) <= 1 && 
            enemy_bullets[i].y >= player.y) {
            enemy_bullets[i].active = 0;
            player.lives--;
            if (player.lives <= 0) game_over = 1;
        }
    }
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].y >= player.y - 1) {
            game_over = 1;
        }
    }
    
    int all_dead = 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) all_dead = 0;
    }
    if (all_dead) game_over = 1;
}

void draw() {
    char screen[HEIGHT][WIDTH + 1];
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == 0 || y == HEIGHT - 1) screen[y][x] = '-';
            else if (x == 0 || x == WIDTH - 1) screen[y][x] = '|';
            else screen[y][x] = ' ';
        }
        screen[y][WIDTH] = '\0';
    }
    
    if (player.x >= 0 && player.x < WIDTH && player.y >= 0 && player.y < HEIGHT)
        screen[player.y][player.x] = 'A';
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].x > 0 && enemies[i].x < WIDTH - 1 &&
            enemies[i].y > 0 && enemies[i].y < HEIGHT - 1) {
            char c = enemies[i].type == 0 ? 'W' : (enemies[i].type == 1 ? 'M' : 'V');
            screen[enemies[i].y][enemies[i].x] = c;
        }
    }
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && bullets[i].x > 0 && bullets[i].x < WIDTH - 1 &&
            bullets[i].y > 0 && bullets[i].y < HEIGHT - 1) {
            screen[bullets[i].y][bullets[i].x] = '|';
        }
    }
    
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemy_bullets[i].active && enemy_bullets[i].x > 0 && 
            enemy_bullets[i].x < WIDTH - 1 && enemy_bullets[i].y > 0 && 
            enemy_bullets[i].y < HEIGHT - 1) {
            screen[enemy_bullets[i].y][enemy_bullets[i].x] = '*';
        }
    }
    
    printf("\033[H\033[J");
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", screen[y]);
    }
    printf("Score: %d  Lives: %d  [A/D or Arrow Keys to move, SPACE to shoot, Q to quit]\n", 
           score, player.lives);
    fflush(stdout);
}

int main() {
    #ifndef _WIN32
    enable_raw_mode();
    #endif
    
    printf("\033[?25l");
    srand(time(NULL));
    init_game();
    
    while (!game_over) {
        if (kbhit()) {
            int ch = getch();
            #ifndef _WIN32
            if (ch == 27) {
                getch();
                ch = getch();
                if (ch == 67 || ch == 'd' || ch == 'D') ch = 'd';
                else if (ch == 68 || ch == 'a' || ch == 'A') ch = 'a';
            }
            #else
            if (ch == 224) {
                ch = getch();
                if (ch == 77) ch = 'd';
                else if (ch == 75) ch = 'a';
            }
            #endif
            
            if (ch == 'a' || ch == 'A') {
                if (player.x > 1) player.x--;
            } else if (ch == 'd' || ch == 'D') {
                if (player.x < WIDTH - 2) player.x++;
            } else if (ch == ' ') {
                shoot_bullet();
            } else if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
        
        update_bullets();
        update_enemies();
        enemy_shoot();
        check_collisions();
        draw();
        
        frame++;
        
        #ifdef _WIN32
        Sleep(50);
        #else
        usleep(50000);
        #endif
    }
    
    printf("\n\n  GAME OVER! Final Score: %d\n\n", score);
    printf("\033[?25h");
    
    return 0;
}
