#include <ncurses.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define WIDTH 80
#define HEIGHT 24
#define MAX_SPEED 4.0
#define ACCEL 0.2
#define FRICTION 0.1
#define TURN_RATE 0.12
#define PI 3.14159265359

typedef struct {
    double x, y, speed, angle;
    int lap, checkpoint;
} Car;

char track[HEIGHT][WIDTH];

void init_track() {
    for (int i = 0; i < HEIGHT; i++)
        for (int j = 0; j < WIDTH; j++)
            track[i][j] = ',';
    
    // Outer walls
    for (int i = 0; i < HEIGHT; i++) { track[i][0] = track[i][WIDTH-1] = '#'; }
    for (int j = 0; j < WIDTH; j++) { track[0][j] = track[HEIGHT-1][j] = '#'; }
    
    // Inner island (oval)
    int cx = WIDTH/2, cy = HEIGHT/2, rx = WIDTH/3, ry = HEIGHT/3;
    for (int i = 1; i < HEIGHT-1; i++) {
        for (int j = 1; j < WIDTH-1; j++) {
            double dx = (j - cx) / (double)rx;
            double dy = (i - cy) / (double)ry;
            double dist = dx*dx + dy*dy;
            track[i][j] = (dist < 0.65) ? '#' : ' ';
        }
    }
    
    // Start/finish line
    for (int i = cy-2; i <= cy+2; i++)
        if (track[i][WIDTH-6] == ' ') track[i][WIDTH-6] = '|';
}

int is_valid(int x, int y) {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && 
           (track[y][x] == ' ' || track[y][x] == '|');
}

void init_car(Car *c) {
    c->x = WIDTH - 10; c->y = HEIGHT/2;
    c->speed = 0; c->angle = PI;
    c->lap = 0; c->checkpoint = 0;
}

void update(Car *c, int up, int down, int left, int right) {
    // Acceleration
    if (up && c->speed < MAX_SPEED) c->speed += ACCEL;
    else if (down && c->speed > 0) c->speed -= ACCEL * 2;
    else if (c->speed > 0) c->speed -= FRICTION;
    
    if (c->speed < 0) c->speed = 0;
    if (c->speed > MAX_SPEED) c->speed = MAX_SPEED;
    
    // Steering
    double turn_factor = c->speed / MAX_SPEED;
    if (left && c->speed > 0.5) c->angle -= TURN_RATE * turn_factor;
    if (right && c->speed > 0.5) c->angle += TURN_RATE * turn_factor;
    
    // Movement
    double nx = c->x + cos(c->angle) * c->speed;
    double ny = c->y + sin(c->angle) * c->speed;
    
    if (is_valid((int)nx, (int)ny)) {
        c->x = nx; c->y = ny;
        
        // Lap detection
        int cx = WIDTH/2, cy = HEIGHT/2;
        if (c->x > cx && c->checkpoint == 0) c->checkpoint = 1;
        else if (c->x < cx && c->checkpoint == 1) c->checkpoint = 2;
        else if (c->x > WIDTH-15 && fabs(c->y - cy) < 3 && c->checkpoint == 2) {
            c->lap++; c->checkpoint = 0;
        }
    } else {
        c->speed *= 0.2; // Crash penalty
    }
}

void draw(Car *c, clock_t start) {
    clear();
    
    // Draw track
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            char ch = track[i][j];
            if (ch == '#') { attron(COLOR_PAIR(1)); mvaddch(i, j, ch); attroff(COLOR_PAIR(1)); }
            else if (ch == '|') { attron(COLOR_PAIR(2)); mvaddch(i, j, ch); attroff(COLOR_PAIR(2)); }
            else if (ch == ',') { attron(COLOR_PAIR(3)); mvaddch(i, j, ch); attroff(COLOR_PAIR(3)); }
            else mvaddch(i, j, ch);
        }
    }
    
    // Draw car
    attron(COLOR_PAIR(4));
    mvaddch((int)c->y, (int)c->x, 'X');
    attroff(COLOR_PAIR(4));
    
    // HUD
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    mvprintw(0, 2, " LAP: %d | SPEED: %3.1f | TIME: %.1fs ", c->lap, c->speed, elapsed);
    mvprintw(HEIGHT-1, 2, " [UP]Accel [DOWN]Brake [LEFT/RIGHT]Steer [Q]uit ");
    
    refresh();
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); curs_set(0);
    
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    
    init_track();
    Car player;
    init_car(&player);
    
    clock_t start_time = clock();
    int running = 1;
    
    while (running) {
        int ch = getch();
        int up = (ch == KEY_UP), down = (ch == KEY_DOWN);
        int left = (ch == KEY_LEFT), right = (ch == KEY_RIGHT);
        
        if (ch == 'q' || ch == 'Q') running = 0;
        
        update(&player, up, down, left, right);
        draw(&player, start_time);
        
        usleep(50000); // ~20 FPS
    }
    
    endwin();
    return 0;
}
