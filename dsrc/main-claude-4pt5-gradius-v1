#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define WIDTH 80
#define HEIGHT 24
#define MAX_BULLETS 50
#define MAX_ENEMIES 20
#define MAX_POWERUPS 5
#define MAX_PARTICLES 100

typedef struct {
    float x, y;
    int active;
} Bullet;

typedef struct {
    float x, y;
    int type;
    int health;
    int active;
    float speed;
    int shoot_timer;
} Enemy;

typedef struct {
    float x, y;
    int type;
    int active;
} PowerUp;

typedef struct {
    float x, y;
    int life;
    char c;
    int active;
} Particle;

typedef struct {
    float x, y;
    int lives;
    int score;
    int power_level;
    int invincible;
} Player;

char screen[HEIGHT][WIDTH];
Player player;
Bullet bullets[MAX_BULLETS];
Bullet enemy_bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
PowerUp powerups[MAX_POWERUPS];
Particle particles[MAX_PARTICLES];
int game_over = 0;
int frame = 0;
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

void clear_screen() {
    printf("\033[2J\033[H");
}

void init_game() {
    player.x = 10;
    player.y = HEIGHT / 2;
    player.lives = 3;
    player.score = 0;
    player.power_level = 0;
    player.invincible = 0;
    
    memset(bullets, 0, sizeof(bullets));
    memset(enemy_bullets, 0, sizeof(enemy_bullets));
    memset(enemies, 0, sizeof(enemies));
    memset(powerups, 0, sizeof(powerups));
    memset(particles, 0, sizeof(particles));
    
    srand(time(NULL));
}

void add_particle(float x, float y, char c) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;
            particles[i].c = c;
            particles[i].life = 5 + rand() % 5;
            particles[i].active = 1;
            break;
        }
    }
}

void shoot_bullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = player.x + 3;
            bullets[i].y = player.y;
            bullets[i].active = 1;
            
            if (player.power_level >= 2) {
                for (int j = 0; j < MAX_BULLETS; j++) {
                    if (!bullets[j].active && j != i) {
                        bullets[j].x = player.x + 3;
                        bullets[j].y = player.y - 1;
                        bullets[j].active = 1;
                        break;
                    }
                }
            }
            break;
        }
    }
}

void spawn_enemy() {
    if (rand() % 100 < 3) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].x = WIDTH - 2;
                enemies[i].y = 2 + rand() % (HEIGHT - 4);
                enemies[i].type = rand() % 3;
                enemies[i].health = enemies[i].type + 1;
                enemies[i].speed = 0.1f + (rand() % 10) / 20.0f;
                enemies[i].shoot_timer = 0;
                enemies[i].active = 1;
                break;
            }
        }
    }
}

void spawn_powerup(float x, float y) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].active) {
            powerups[i].x = x;
            powerups[i].y = y;
            powerups[i].type = rand() % 2;
            powerups[i].active = 1;
            break;
        }
    }
}

void update_bullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].x += 1.5f;
            if (bullets[i].x >= WIDTH - 1) {
                bullets[i].active = 0;
            }
        }
    }
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (enemy_bullets[i].active) {
            enemy_bullets[i].x -= 0.8f;
            if (enemy_bullets[i].x <= 0) {
                enemy_bullets[i].active = 0;
            }
            
            if (!player.invincible &&
                (int)enemy_bullets[i].x >= (int)player.x && 
                (int)enemy_bullets[i].x <= (int)player.x + 2 &&
                (int)enemy_bullets[i].y >= (int)player.y - 1 && 
                (int)enemy_bullets[i].y <= (int)player.y + 1) {
                player.lives--;
                player.invincible = 60;
                enemy_bullets[i].active = 0;
                for (int j = 0; j < 10; j++) {
                    add_particle(player.x, player.y, '*');
                }
                if (player.lives <= 0) game_over = 1;
            }
        }
    }
}

void update_enemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            enemies[i].x -= enemies[i].speed;
            
            if (enemies[i].type == 1) {
                enemies[i].y += 0.1f * ((frame % 60) < 30 ? 1 : -1);
            }
            
            enemies[i].shoot_timer++;
            if (enemies[i].shoot_timer > 40 && rand() % 100 < 5) {
                for (int j = 0; j < MAX_BULLETS; j++) {
                    if (!enemy_bullets[j].active) {
                        enemy_bullets[j].x = enemies[i].x;
                        enemy_bullets[j].y = enemies[i].y;
                        enemy_bullets[j].active = 1;
                        enemies[i].shoot_timer = 0;
                        break;
                    }
                }
            }
            
            if (enemies[i].x < 0) {
                enemies[i].active = 0;
            }
            
            if (!player.invincible &&
                (int)enemies[i].x >= (int)player.x && 
                (int)enemies[i].x <= (int)player.x + 2 &&
                (int)enemies[i].y == (int)player.y) {
                player.lives--;
                player.invincible = 60;
                enemies[i].active = 0;
                for (int j = 0; j < 15; j++) {
                    add_particle(enemies[i].x, enemies[i].y, '#');
                }
                if (player.lives <= 0) game_over = 1;
            }
        }
    }
}

void update_powerups() {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (powerups[i].active) {
            powerups[i].x -= 0.3f;
            
            if (powerups[i].x < 0) {
                powerups[i].active = 0;
            }
            
            if ((int)powerups[i].x >= (int)player.x && 
                (int)powerups[i].x <= (int)player.x + 2 &&
                (int)powerups[i].y == (int)player.y) {
                if (powerups[i].type == 0) {
                    player.power_level = (player.power_level < 5) ? player.power_level + 1 : 5;
                } else {
                    player.score += 100;
                }
                powerups[i].active = 0;
                for (int j = 0; j < 5; j++) {
                    add_particle(powerups[i].x, powerups[i].y, '+');
                }
            }
        }
    }
}

void update_particles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].life--;
            particles[i].x += (rand() % 3 - 1) * 0.5f;
            particles[i].y += (rand() % 3 - 1) * 0.3f;
            if (particles[i].life <= 0) {
                particles[i].active = 0;
            }
        }
    }
}

void check_collisions() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active &&
                    (int)bullets[i].x == (int)enemies[j].x &&
                    (int)bullets[i].y == (int)enemies[j].y) {
                    bullets[i].active = 0;
                    enemies[j].health--;
                    
                    add_particle(enemies[j].x, enemies[j].y, '*');
                    
                    if (enemies[j].health <= 0) {
                        player.score += (enemies[j].type + 1) * 10;
                        for (int k = 0; k < 10; k++) {
                            add_particle(enemies[j].x, enemies[j].y, '#');
                        }
                        if (rand() % 100 < 30) {
                            spawn_powerup(enemies[j].x, enemies[j].y);
                        }
                        enemies[j].active = 0;
                    }
                    break;
                }
            }
        }
    }
}

void render() {
    memset(screen, ' ', sizeof(screen));
    
    for (int y = 0; y < HEIGHT; y++) {
        if ((frame / 2 + y) % 4 == 0) {
            screen[y][(frame / 2) % WIDTH] = '.';
        }
        if ((frame / 3 + y) % 7 == 0) {
            screen[y][(frame / 3) % WIDTH] = ':';
        }
    }
    
    char ship[3][4] = {
        " ^ ",
        "=>)",
        " v "
    };
    
    if ((int)player.y - 1 >= 0 && (int)player.y + 1 < HEIGHT) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                int px = (int)player.x + dx;
                int py = (int)player.y + dy;
                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                    if (player.invincible && (frame % 6) < 3) {
                        screen[py][px] = ' ';
                    } else {
                        screen[py][px] = ship[dy + 1][dx];
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            int x = (int)bullets[i].x;
            int y = (int)bullets[i].y;
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                screen[y][x] = '-';
            }
        }
    }
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (enemy_bullets[i].active) {
            int x = (int)enemy_bullets[i].x;
            int y = (int)enemy_bullets[i].y;
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                screen[y][x] = '~';
            }
        }
    }
    
    char enemy_chars[] = {'o', '@', 'X'};
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            int x = (int)enemies[i].x;
            int y = (int)enemies[i].y;
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                screen[y][x] = enemy_chars[enemies[i].type];
            }
        }
    }
    
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (powerups[i].active) {
            int x = (int)powerups[i].x;
            int y = (int)powerups[i].y;
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                screen[y][x] = powerups[i].type == 0 ? 'P' : '$';
            }
        }
    }
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            int x = (int)particles[i].x;
            int y = (int)particles[i].y;
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                screen[y][x] = particles[i].c;
            }
        }
    }
    
    printf("\033[H");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            char c = screen[y][x];
            if (c == '^' || c == 'v' || c == '>' || c == ')' || c == '=') {
                printf("\033[36m%c\033[0m", c);
            } else if (c == '-') {
                printf("\033[33m%c\033[0m", c);
            } else if (c == 'o' || c == '@' || c == 'X') {
                printf("\033[31m%c\033[0m", c);
            } else if (c == '~') {
                printf("\033[35m%c\033[0m", c);
            } else if (c == 'P') {
                printf("\033[32m%c\033[0m", c);
            } else if (c == '$') {
                printf("\033[33m%c\033[0m", c);
            } else if (c == '*' || c == '#' || c == '+') {
                printf("\033[37m%c\033[0m", c);
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    
    printf("\033[33mScore: %d  Lives: %d  Power: %d\033[0m", 
           player.score, player.lives, player.power_level);
    fflush(stdout);
}

int main() {
    enable_raw_mode();
    clear_screen();
    printf("\033[?25l");
    
    init_game();
    
    char input;
    int shoot_cooldown = 0;
    
    printf("\033[H\033[2J");
    printf("\n\n");
    printf("  ╔═══════════════════════════════════════╗\n");
    printf("  ║     GRADIUS-STYLE SPACE SHOOTER      ║\n");
    printf("  ╠═══════════════════════════════════════╣\n");
    printf("  ║                                       ║\n");
    printf("  ║  Controls:                            ║\n");
    printf("  ║    W/A/S/D - Move                     ║\n");
    printf("  ║    SPACE   - Shoot                    ║\n");
    printf("  ║    Q       - Quit                     ║\n");
    printf("  ║                                       ║\n");
    printf("  ║  Collect P for power-ups!             ║\n");
    printf("  ║  Collect $ for bonus points!          ║\n");
    printf("  ║                                       ║\n");
    printf("  ║  Press SPACE to start...              ║\n");
    printf("  ║                                       ║\n");
    printf("  ╚═══════════════════════════════════════╝\n");
    
    while (read(STDIN_FILENO, &input, 1) == 1) {
        if (input == ' ') break;
        if (input == 'q') {
            printf("\033[?25h\033[2J\033[H");
            return 0;
        }
    }
    
    clear_screen();
    
    while (!game_over) {
        if (read(STDIN_FILENO, &input, 1) == 1) {
            switch (input) {
                case 'w': if (player.y > 1) player.y -= 1; break;
                case 's': if (player.y < HEIGHT - 2) player.y += 1; break;
                case 'a': if (player.x > 1) player.x -= 1; break;
                case 'd': if (player.x < WIDTH - 5) player.x += 1; break;
                case ' ': 
                    if (shoot_cooldown <= 0) {
                        shoot_bullet();
                        shoot_cooldown = 8 - player.power_level;
                    }
                    break;
                case 'q': game_over = 1; break;
            }
        }
        
        if (shoot_cooldown > 0) shoot_cooldown--;
        if (player.invincible > 0) player.invincible--;
        
        spawn_enemy();
        update_bullets();
        update_enemies();
        update_powerups();
        update_particles();
        check_collisions();
        
        render();
        
        frame++;
        usleep(33333);
    }
    
    printf("\033[?25h");
    printf("\n\n  GAME OVER! Final Score: %d\n\n", player.score);
    
    return 0;
}
