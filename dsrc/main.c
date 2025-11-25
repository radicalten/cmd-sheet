#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
#endif

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 30
#define TRACK_WIDTH 60
#define TRACK_HEIGHT 25
#define MAX_CARS 3
#define MAX_SPEED 1.5
#define ACCELERATION 0.12
#define FRICTION 0.93
#define TURN_SPEED 0.12
#define LAPS_TO_WIN 3

typedef struct {
    double x, y;
    double vx, vy;
    double angle;
    int lap;
    int checkpoint;
    char symbol;
    int is_player;
    int finished;
    double finish_time;
} Car;

typedef struct {
    char grid[TRACK_HEIGHT][TRACK_WIDTH];
} Track;

Car cars[MAX_CARS];
Track track;
char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
double game_time = 0;

#ifndef _WIN32
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(0, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(0, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSAFLUSH, &raw);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
}

int kbhit() {
    int bytes;
    ioctl(0, FIONREAD, &bytes);
    return bytes > 0;
}
#endif

void sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

void init_track() {
    int center_x = TRACK_WIDTH / 2;
    int center_y = TRACK_HEIGHT / 2;
    
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        for (int x = 0; x < TRACK_WIDTH; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            
            double outer = (dx*dx)/(28.0*28.0) + (dy*dy)/(11.0*11.0);
            double inner = (dx*dx)/(18.0*18.0) + (dy*dy)/(7.0*7.0);
            
            if (outer <= 1.0 && inner >= 1.0) {
                track.grid[y][x] = '.';
            } else {
                track.grid[y][x] = '#';
            }
        }
    }
    
    for (int y = center_y - 11; y <= center_y + 11; y++) {
        if (y >= 0 && y < TRACK_HEIGHT) {
            if (track.grid[y][center_x + 18] == '.') {
                track.grid[y][center_x + 18] = '=';
            }
            if (track.grid[y][center_x - 18] == '.') {
                track.grid[y][center_x - 18] = '|';
            }
        }
    }
}

void init_cars() {
    char symbols[] = {'P', 'A', 'B'};
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].x = 48;
        cars[i].y = 12 + (i - 1) * 2;
        cars[i].vx = cars[i].vy = 0;
        cars[i].angle = 0;
        cars[i].lap = 0;
        cars[i].checkpoint = 0;
        cars[i].symbol = symbols[i];
        cars[i].is_player = (i == 0);
        cars[i].finished = 0;
        cars[i].finish_time = 0;
    }
}

int is_valid_position(int x, int y) {
    if (x < 0 || x >= TRACK_WIDTH || y < 0 || y >= TRACK_HEIGHT)
        return 0;
    return track.grid[y][x] != '#';
}

void update_ai(Car* car) {
    if (car->finished) return;
    
    int center_x = TRACK_WIDTH / 2;
    int center_y = TRACK_HEIGHT / 2;
    
    double target_x = (car->checkpoint == 0) ? center_x - 18 : center_x + 18;
    double target_y = center_y;
    
    double dx = target_x - car->x;
    double dy = target_y - car->y;
    double target_angle = atan2(dy, dx);
    
    double angle_diff = target_angle - car->angle;
    while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2 * M_PI;
    
    if (angle_diff > 0.1) {
        car->angle += TURN_SPEED * 0.7;
    } else if (angle_diff < -0.1) {
        car->angle -= TURN_SPEED * 0.7;
    }
    
    car->vx += cos(car->angle) * ACCELERATION * 0.85;
    car->vy += sin(car->angle) * ACCELERATION * 0.85;
}

void update_car(Car* car, int up, int down, int left, int right) {
    if (car->finished) return;
    
    if (car->is_player) {
        if (left) car->angle -= TURN_SPEED;
        if (right) car->angle += TURN_SPEED;
        if (up) {
            car->vx += cos(car->angle) * ACCELERATION;
            car->vy += sin(car->angle) * ACCELERATION;
        }
        if (down) {
            car->vx *= 0.85;
            car->vy *= 0.85;
        }
    } else {
        update_ai(car);
    }
    
    car->vx *= FRICTION;
    car->vy *= FRICTION;
    
    double speed = sqrt(car->vx * car->vx + car->vy * car->vy);
    if (speed > MAX_SPEED) {
        car->vx = (car->vx / speed) * MAX_SPEED;
        car->vy = (car->vy / speed) * MAX_SPEED;
    }
    
    double new_x = car->x + car->vx;
    double new_y = car->y + car->vy;
    
    if (is_valid_position((int)new_x, (int)new_y)) {
        car->x = new_x;
        car->y = new_y;
        
        char cell = track.grid[(int)car->y][(int)car->x];
        if (cell == '|' && car->checkpoint == 0) {
            car->checkpoint = 1;
        } else if (cell == '=' && car->checkpoint == 1) {
            car->checkpoint = 0;
            car->lap++;
            if (car->lap >= LAPS_TO_WIN) {
                car->finished = 1;
                car->finish_time = game_time;
            }
        }
    } else {
        car->vx *= -0.4;
        car->vy *= -0.4;
    }
}

void render() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x] = ' ';
        }
    }
    
    for (int y = 0; y < TRACK_HEIGHT; y++) {
        for (int x = 0; x < TRACK_WIDTH; x++) {
            screen[y][x] = track.grid[y][x];
        }
    }
    
    for (int i = 0; i < MAX_CARS; i++) {
        int cx = (int)cars[i].x;
        int cy = (int)cars[i].y;
        if (cx >= 0 && cx < TRACK_WIDTH && cy >= 0 && cy < TRACK_HEIGHT) {
            screen[cy][cx] = cars[i].symbol;
        }
    }
    
    int hud_x = TRACK_WIDTH + 2;
    char info[40];
    
    sprintf(info, "╔════════════════╗");
    for (int i = 0; info[i]; i++) screen[0][hud_x + i] = info[i];
    sprintf(info, "║ SUPER SPRINT  ║");
    for (int i = 0; info[i]; i++) screen[1][hud_x + i] = info[i];
    sprintf(info, "╚════════════════╝");
    for (int i = 0; info[i]; i++) screen[2][hud_x + i] = info[i];
    
    sprintf(info, "Time: %.1fs", game_time);
    for (int i = 0; info[i]; i++) screen[4][hud_x + i] = info[i];
    
    for (int i = 0; i < MAX_CARS; i++) {
        int line = 6 + i * 2;
        sprintf(info, "%c: Lap %d/%d", cars[i].symbol, cars[i].lap, LAPS_TO_WIN);
        for (int j = 0; info[j]; j++) screen[line][hud_x + j] = info[j];
        
        if (cars[i].finished) {
            sprintf(info, "  FINISH! %.1fs", cars[i].finish_time);
            for (int j = 0; info[j]; j++) screen[line+1][hud_x + j] = info[j];
        }
    }
    
    sprintf(info, "┌──────────────┐");
    for (int i = 0; info[i]; i++) screen[13][hud_x + i] = info[i];
    sprintf(info, "│  Controls:   │");
    for (int i = 0; info[i]; i++) screen[14][hud_x + i] = info[i];
    sprintf(info, "│  W - Gas     │");
    for (int i = 0; info[i]; i++) screen[15][hud_x + i] = info[i];
    sprintf(info, "│  S - Brake   │");
    for (int i = 0; info[i]; i++) screen[16][hud_x + i] = info[i];
    sprintf(info, "│  A - Left    │");
    for (int i = 0; info[i]; i++) screen[17][hud_x + i] = info[i];
    sprintf(info, "│  D - Right   │");
    for (int i = 0; info[i]; i++) screen[18][hud_x + i] = info[i];
    sprintf(info, "│  Q - Quit    │");
    for (int i = 0; info[i]; i++) screen[19][hud_x + i] = info[i];
    sprintf(info, "└──────────────┘");
    for (int i = 0; info[i]; i++) screen[20][hud_x + i] = info[i];
    
    clear_screen();
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            putchar(screen[y][x]);
        }
        putchar('\n');
    }
    fflush(stdout);
}

int main() {
    srand(time(NULL));
    
#ifndef _WIN32
    enable_raw_mode();
#endif
    
    init_track();
    init_cars();
    
    int running = 1;
    int up = 0, down = 0, left = 0, right = 0;
    clock_t start_time = clock();
    
    printf("SUPER SPRINT - Press any key to start...\n");
    getchar();
    
    while (running) {
#ifdef _WIN32
        if (_kbhit()) {
            int ch = _getch();
#else
        if (kbhit()) {
            int ch = getchar();
#endif
            up = down = left = right = 0;
            
            if (ch == 'q' || ch == 'Q') running = 0;
            if (ch == 'w' || ch == 'W') up = 1;
            if (ch == 's' || ch == 'S') down = 1;
            if (ch == 'a' || ch == 'A') left = 1;
            if (ch == 'd' || ch == 'D') right = 1;
        }
        
        game_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        
        for (int i = 0; i < MAX_CARS; i++) {
            update_car(&cars[i], up, down, left, right);
        }
        
        render();
        
        int all_finished = 1;
        for (int i = 0; i < MAX_CARS; i++) {
            if (!cars[i].finished) all_finished = 0;
        }
        
        if (all_finished) running = 0;
        
        sleep_ms(50);
    }
    
#ifndef _WIN32
    disable_raw_mode();
#endif
    
    printf("\n\n=== RACE RESULTS ===\n");
    for (int i = 0; i < MAX_CARS; i++) {
        printf("%c: Finished in %.2f seconds\n", 
               cars[i].symbol, cars[i].finish_time);
    }
    printf("\nThanks for playing!\n");
    
    return 0;
}
