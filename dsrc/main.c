#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define TRACK_POINTS 60
#define MAX_ENEMIES 3
#define LAPS_TO_WIN 3

typedef struct {
    float x, y;
} Point;

typedef struct {
    float x, y;
    float angle;
    float speed;
    int lap;
    int checkpoint;
    int last_checkpoint;
} Car;

char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
Point track[TRACK_POINTS];
Car player;
Car enemies[MAX_ENEMIES];
time_t start_time;
int game_over = 0;
struct termios orig_termios;

void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H");
}

void setup_terminal() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    printf("\033[?25l\033[2J");
}

int kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

void init_track() {
    float cx = SCREEN_WIDTH / 2.0f;
    float cy = SCREEN_HEIGHT / 2.0f;
    float rx = 30.0f;
    float ry = 9.0f;
    
    for (int i = 0; i < TRACK_POINTS; i++) {
        float angle = (2.0f * M_PI * i) / TRACK_POINTS;
        track[i].x = cx + rx * cos(angle);
        track[i].y = cy + ry * sin(angle);
    }
}

void init_cars() {
    player.x = track[0].x + 3;
    player.y = track[0].y;
    player.angle = 0;
    player.speed = 0;
    player.lap = 0;
    player.checkpoint = 0;
    player.last_checkpoint = 0;
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        int start_pos = (i + 1) * (TRACK_POINTS / (MAX_ENEMIES + 1));
        enemies[i].x = track[start_pos].x + 3;
        enemies[i].y = track[start_pos].y;
        enemies[i].angle = 0;
        enemies[i].speed = 0.4f + (rand() % 10) / 30.0f;
        enemies[i].lap = 0;
        enemies[i].checkpoint = start_pos;
        enemies[i].last_checkpoint = start_pos;
    }
}

void clear_screen() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x] = ' ';
        }
    }
}

void draw_pixel(int x, int y, char c) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        screen[y][x] = c;
    }
}

void draw_circle(float cx, float cy, float radius, char c) {
    for (float angle = 0; angle < 2 * M_PI; angle += 0.1f) {
        int x = (int)(cx + radius * cos(angle));
        int y = (int)(cy + radius * sin(angle));
        draw_pixel(x, y, c);
    }
}

void draw_track() {
    // Draw outer and inner boundaries
    for (int i = 0; i < TRACK_POINTS; i++) {
        int next = (i + 1) % TRACK_POINTS;
        float dx = track[next].x - track[i].x;
        float dy = track[next].y - track[i].y;
        float dist = sqrt(dx*dx + dy*dy);
        
        for (float t = 0; t < 1.0f; t += 0.05f / dist) {
            int x = (int)(track[i].x + dx * t);
            int y = (int)(track[i].y + dy * t);
            
            // Outer boundary
            draw_pixel(x, y, '#');
            
            // Inner boundary
            float perpx = -dy / dist * 6;
            float perpy = dx / dist * 6;
            draw_pixel((int)(x + perpx), (int)(y + perpy), '#');
        }
    }
    
    // Start/finish line
    float dx = track[1].x - track[0].x;
    float dy = track[1].y - track[0].y;
    float dist = sqrt(dx*dx + dy*dy);
    float perpx = -dy / dist * 6;
    float perpy = dx / dist * 6;
    
    for (int i = 0; i <= 6; i++) {
        int x = (int)(track[0].x + perpx * i / 6);
        int y = (int)(track[0].y + perpy * i / 6);
        draw_pixel(x, y, (i % 2) ? '|' : '=');
    }
}

void draw_car(Car *car, char c) {
    int x = (int)(car->x + 0.5f);
    int y = (int)(car->y + 0.5f);
    draw_pixel(x, y, c);
    
    // Draw direction indicator
    int dx = (int)(car->x + cos(car->angle) * 1.5f + 0.5f);
    int dy = (int)(car->y + sin(car->angle) * 1.5f + 0.5f);
    draw_pixel(dx, dy, '.');
}

void update_player(char input) {
    // Acceleration/Braking
    if (input == 'w' || input == 'W') {
        player.speed += 0.15f;
        if (player.speed > 2.0f) player.speed = 2.0f;
    } else if (input == 's' || input == 'S') {
        player.speed -= 0.15f;
        if (player.speed < -0.8f) player.speed = -0.8f;
    } else {
        player.speed *= 0.92f; // Friction
    }
    
    // Steering
    if (fabs(player.speed) > 0.1f) {
        if (input == 'a' || input == 'A') {
            player.angle -= 0.12f * (player.speed / fabs(player.speed));
        } else if (input == 'd' || input == 'D') {
            player.angle += 0.12f * (player.speed / fabs(player.speed));
        }
    }
    
    // Update position
    player.x += cos(player.angle) * player.speed;
    player.y += sin(player.angle) * player.speed;
    
    // Find closest track point
    int closest = 0;
    float min_dist = 999999;
    for (int i = 0; i < TRACK_POINTS; i++) {
        float dx = player.x - track[i].x - 3;
        float dy = player.y - track[i].y;
        float dist = dx*dx + dy*dy;
        if (dist < min_dist) {
            min_dist = dist;
            closest = i;
        }
    }
    
    // Lap detection - crossing start line
    if (player.last_checkpoint > TRACK_POINTS * 3/4 && closest < TRACK_POINTS / 4) {
        player.lap++;
        if (player.lap >= LAPS_TO_WIN) {
            game_over = 1;
        }
    }
    
    player.last_checkpoint = player.checkpoint;
    player.checkpoint = closest;
}

void update_enemy(Car *enemy) {
    int target = (enemy->checkpoint + 2) % TRACK_POINTS;
    
    float dx = track[target].x + 3 - enemy->x;
    float dy = track[target].y - enemy->y;
    float dist = sqrt(dx*dx + dy*dy);
    
    if (dist < 3.0f) {
        enemy->last_checkpoint = enemy->checkpoint;
        enemy->checkpoint = target;
    }
    
    enemy->angle = atan2(dy, dx);
    enemy->x += cos(enemy->angle) * enemy->speed;
    enemy->y += sin(enemy->angle) * enemy->speed;
    
    // Lap detection
    if (enemy->last_checkpoint > TRACK_POINTS * 3/4 && 
        enemy->checkpoint < TRACK_POINTS / 4) {
        enemy->lap++;
    }
}

void draw_hud() {
    char hud[100];
    time_t elapsed = time(NULL) - start_time;
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;
    
    sprintf(hud, " LAP:%d/%d TIME:%02d:%02d SPEED:%.1f [W]Gas [S]Brake [A/D]Steer [Q]Quit ", 
            player.lap, LAPS_TO_WIN, minutes, seconds, player.speed);
    
    for (int i = 0; i < strlen(hud) && i < SCREEN_WIDTH; i++) {
        screen[0][i] = hud[i];
    }
    
    // Position info
    sprintf(hud, " POS: P=%d%% ", (player.checkpoint * 100) / TRACK_POINTS);
    for (int i = 0; i < strlen(hud) && i < SCREEN_WIDTH; i++) {
        screen[SCREEN_HEIGHT-1][i] = hud[i];
    }
}

void render() {
    printf("\033[H");
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        fwrite(screen[y], 1, SCREEN_WIDTH, stdout);
        putchar('\n');
    }
    fflush(stdout);
}

void victory_screen() {
    clear_screen();
    
    char *title[] = {
        "##     ## ####  ######  ########  #######  ########  ##    ## ",
        "##     ##  ##  ##    ##    ##    ##     ## ##     ##  ##  ##  ",
        "##     ##  ##  ##          ##    ##     ## ##     ##   ####   ",
        "##     ##  ##  ##          ##    ##     ## ########     ##    ",
        " ##   ##   ##  ##          ##    ##     ## ##   ##      ##    ",
        "  ## ##    ##  ##    ##    ##    ##     ## ##    ##     ##    ",
        "   ###    ####  ######     ##     #######  ##     ##    ##    "
    };
    
    time_t elapsed = time(NULL) - start_time;
    char time_msg[50];
    sprintf(time_msg, "Final Time: %02ld:%02ld", elapsed / 60, elapsed % 60);
    char msg[] = "Press any key to exit...";
    
    int start_y = 5;
    for (int line = 0; line < 7; line++) {
        int start_x = (SCREEN_WIDTH - strlen(title[line])) / 2;
        if (start_x < 0) start_x = 0;
        for (int i = 0; title[line][i] && start_x + i < SCREEN_WIDTH; i++) {
            draw_pixel(start_x + i, start_y + line, title[line][i]);
        }
    }
    
    int msg_x = (SCREEN_WIDTH - strlen(time_msg)) / 2;
    for (int i = 0; time_msg[i]; i++)
        draw_pixel(msg_x + i, 15, time_msg[i]);
    
    msg_x = (SCREEN_WIDTH - strlen(msg)) / 2;
    for (int i = 0; msg[i]; i++)
        draw_pixel(msg_x + i, 18, msg[i]);
    
    render();
    
    // Wait for keypress
    getchar();
}

void show_intro() {
    printf("\033[2J\033[H");
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════════════════════╗\n");
    printf("  ║                    TOP-DOWN RACING CHAMPIONSHIP                       ║\n");
    printf("  ╚═══════════════════════════════════════════════════════════════════════╝\n\n");
    printf("  OBJECTIVE: Complete %d laps around the track before the competition!\n\n", LAPS_TO_WIN);
    printf("  CONTROLS:\n");
    printf("    W - Accelerate\n");
    printf("    S - Brake/Reverse\n");
    printf("    A - Turn Left\n");
    printf("    D - Turn Right\n");
    printf("    Q - Quit Game\n\n");
    printf("  LEGEND:\n");
    printf("    P - Your car (with direction indicator .)\n");
    printf("    E - Enemy cars\n");
    printf("    # - Track boundaries\n");
    printf("    | - Start/Finish line\n\n");
    printf("  Press any key to start racing...\n");
    
    getchar();
}

int main() {
    srand(time(NULL));
    setup_terminal();
    
    show_intro();
    
    init_track();
    init_cars();
    start_time = time(NULL);
    
    char input = 0;
    
    while (!game_over) {
        if (kbhit()) {
            input = getchar();
            if (input == 'q' || input == 'Q') break;
        }
        
        clear_screen();
        draw_track();
        
        update_player(input);
        input = 0;
        
        for (int i = 0; i < MAX_ENEMIES; i++) {
            update_enemy(&enemies[i]);
            draw_car(&enemies[i], 'E');
        }
        
        draw_car(&player, 'P');
        draw_hud();
        render();
        
        usleep(50000); // ~20 FPS
    }
    
    if (game_over) {
        victory_screen();
    }
    
    return 0;
}
