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

// Constants
#define WIDTH 80
#define HEIGHT 35
#define MAX_ENEMIES 3
#define LAPS_TO_WIN 3
#define PI 3.14159265359

// Track parameters
#define TRACK_CENTER_X 40
#define TRACK_CENTER_Y 17
#define TRACK_RADIUS_X 30
#define TRACK_RADIUS_Y 12
#define TRACK_WIDTH 6

// Colors (ANSI escape codes)
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BG_GREEN "\033[42m"
#define COLOR_BG_GRAY "\033[100m"

typedef struct {
    double x, y;
    double angle;
    double speed;
    int lap;
    int checkpoint;
    char *color;
} Car;

char screen[HEIGHT][WIDTH];
Car player;
Car enemies[MAX_ENEMIES];
time_t start_time;
int game_over = 0;
int player_won = 0;

// Platform-specific functions
#ifdef _WIN32
void setup_terminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

int kbhit() {
    return _kbhit();
}

char get_key() {
    return _getch();
}

void sleep_ms(int ms) {
    Sleep(ms);
}
#else
void setup_terminal() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

char get_key() {
    return getchar();
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}
#endif

void clear_screen() {
    printf("\033[2J\033[H");
}

void init_screen() {
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            screen[y][x] = ' ';
        }
    }
}

int is_on_track(double x, double y) {
    double dx = (x - TRACK_CENTER_X) / TRACK_RADIUS_X;
    double dy = (y - TRACK_CENTER_Y) / TRACK_RADIUS_Y;
    double dist = sqrt(dx * dx + dy * dy);
    
    double inner = 1.0 - (TRACK_WIDTH / 2.0) / TRACK_RADIUS_Y;
    double outer = 1.0 + (TRACK_WIDTH / 2.0) / TRACK_RADIUS_Y;
    
    return dist >= inner && dist <= outer;
}

void init_car(Car *car, double angle_offset, char *color, int is_player) {
    car->angle = angle_offset;
    car->x = TRACK_CENTER_X + cos(car->angle) * TRACK_RADIUS_X;
    car->y = TRACK_CENTER_Y + sin(car->angle) * TRACK_RADIUS_Y;
    car->speed = 0;
    car->lap = 0;
    car->checkpoint = 0;
    car->color = color;
}

void draw_track() {
    // Draw track
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            if(is_on_track(x, y)) {
                screen[y][x] = '.';
            } else {
                screen[y][x] = '#';
            }
        }
    }
    
    // Draw finish line
    for(int i = -2; i <= 2; i++) {
        int x = TRACK_CENTER_X + TRACK_RADIUS_X;
        int y = TRACK_CENTER_Y + i;
        if(y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
            screen[y][x] = '|';
        }
    }
}

void update_checkpoint(Car *car) {
    double angle = atan2(car->y - TRACK_CENTER_Y, car->x - TRACK_CENTER_X);
    if(angle < 0) angle += 2 * PI;
    
    int new_checkpoint = (int)(angle / (PI / 2));
    
    if(new_checkpoint != car->checkpoint) {
        if((car->checkpoint == 3 && new_checkpoint == 0)) {
            car->lap++;
        }
        car->checkpoint = new_checkpoint;
    }
}

void update_car(Car *car, int is_player) {
    if(!is_player) {
        // Simple AI: follow the track
        double target_angle = atan2(car->y - TRACK_CENTER_Y, car->x - TRACK_CENTER_X);
        target_angle += 0.15;
        
        double target_x = TRACK_CENTER_X + cos(target_angle) * TRACK_RADIUS_X;
        double target_y = TRACK_CENTER_Y + sin(target_angle) * TRACK_RADIUS_Y;
        
        double dx = target_x - car->x;
        double dy = target_y - car->y;
        double dist = sqrt(dx * dx + dy * dy);
        
        if(dist > 0.1) {
            car->angle = atan2(dy, dx);
            car->speed = 0.8;
        }
    }
    
    // Update position
    double new_x = car->x + cos(car->angle) * car->speed;
    double new_y = car->y + sin(car->angle) * car->speed;
    
    // Check if new position is on track
    if(is_on_track(new_x, new_y)) {
        car->x = new_x;
        car->y = new_y;
    } else {
        car->speed *= 0.5; // Slow down when hitting walls
    }
    
    // Apply friction
    car->speed *= 0.95;
    
    update_checkpoint(car);
    
    // Check for win/lose
    if(car->lap >= LAPS_TO_WIN) {
        game_over = 1;
        if(is_player) {
            player_won = 1;
        }
    }
}

void draw_car(Car *car) {
    int x = (int)(car->x + 0.5);
    int y = (int)(car->y + 0.5);
    
    if(x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        screen[y][x] = 'O';
    }
}

void render() {
    clear_screen();
    
    // Print screen buffer with colors
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            char c = screen[y][x];
            
            // Check if any car is at this position
            int is_player_pos = ((int)(player.x + 0.5) == x && (int)(player.y + 0.5) == y);
            int enemy_idx = -1;
            
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if((int)(enemies[i].x + 0.5) == x && (int)(enemies[i].y + 0.5) == y) {
                    enemy_idx = i;
                    break;
                }
            }
            
            if(is_player_pos) {
                printf("%sO%s", COLOR_GREEN, COLOR_RESET);
            } else if(enemy_idx >= 0) {
                printf("%sO%s", enemies[enemy_idx].color, COLOR_RESET);
            } else if(c == '#') {
                printf("%s#%s", COLOR_YELLOW, COLOR_RESET);
            } else if(c == '.') {
                printf("%s.%s", COLOR_WHITE, COLOR_RESET);
            } else if(c == '|') {
                printf("%s|%s", COLOR_CYAN, COLOR_RESET);
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    
    // Print UI
    time_t elapsed = time(NULL) - start_time;
    printf("\n%s[PLAYER]%s Lap: %d/%d | Speed: %.1f | ", 
           COLOR_GREEN, COLOR_RESET, player.lap, LAPS_TO_WIN, player.speed);
    printf("Time: %ld:%02ld\n", elapsed / 60, elapsed % 60);
    
    for(int i = 0; i < MAX_ENEMIES; i++) {
        printf("%s[CPU %d]%s Lap: %d/%d | ", 
               enemies[i].color, i+1, COLOR_RESET, enemies[i].lap, LAPS_TO_WIN);
    }
    printf("\n");
    printf("\nControls: W=Accelerate S=Brake A=Left D=Right Q=Quit\n");
}

void show_victory_screen() {
    clear_screen();
    time_t elapsed = time(NULL) - start_time;
    
    printf("\n\n\n");
    if(player_won) {
        printf("        %s╔═══════════════════════════════╗%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║      🏆  VICTORY!  🏆        ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║   You won the race!          ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s║   Time: %ld:%02ld                 ║%s\n", 
               COLOR_GREEN, elapsed / 60, elapsed % 60, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_GREEN, COLOR_RESET);
        printf("        %s╚═══════════════════════════════╝%s\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("        %s╔═══════════════════════════════╗%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║         GAME OVER             ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║   An opponent won the race!   ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║   Better luck next time!      ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s║                               ║%s\n", COLOR_RED, COLOR_RESET);
        printf("        %s╚═══════════════════════════════╝%s\n", COLOR_RED, COLOR_RESET);
    }
    printf("\n\n");
}

int main() {
    setup_terminal();
    srand(time(NULL));
    
    // Initialize game
    init_car(&player, 0, COLOR_GREEN, 1);
    init_car(&enemies[0], -0.3, COLOR_RED, 0);
    init_car(&enemies[1], -0.6, COLOR_MAGENTA, 0);
    init_car(&enemies[2], -0.9, COLOR_YELLOW, 0);
    
    start_time = time(NULL);
    
    clear_screen();
    printf("\n\n        %sRACING GAME%s\n\n", COLOR_CYAN, COLOR_RESET);
    printf("        Complete %d laps to win!\n", LAPS_TO_WIN);
    printf("        Press any key to start...\n\n");
    getchar();
    
    start_time = time(NULL);
    
    // Game loop
    while(!game_over) {
        init_screen();
        draw_track();
        
        // Handle input
        if(kbhit()) {
            char key = get_key();
            switch(key) {
                case 'w':
                case 'W':
                    player.speed += 0.3;
                    if(player.speed > 2.0) player.speed = 2.0;
                    break;
                case 's':
                case 'S':
                    player.speed -= 0.3;
                    if(player.speed < -1.0) player.speed = -1.0;
                    break;
                case 'a':
                case 'A':
                    player.angle -= 0.2;
                    break;
                case 'd':
                case 'D':
                    player.angle += 0.2;
                    break;
                case 'q':
                case 'Q':
                    game_over = 1;
                    break;
            }
        }
        
        // Update game state
        update_car(&player, 1);
        for(int i = 0; i < MAX_ENEMIES; i++) {
            update_car(&enemies[i], 0);
        }
        
        // Draw cars
        draw_car(&player);
        for(int i = 0; i < MAX_ENEMIES; i++) {
            draw_car(&enemies[i]);
        }
        
        // Render
        render();
        
        sleep_ms(50);
    }
    
    // Show victory/defeat screen
    show_victory_screen();
    
    return 0;
}
