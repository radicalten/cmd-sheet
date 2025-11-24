#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

#define WIDTH  118
#define HEIGHT  40
#define NUM_CARS 3
#define NUM_NITROS 8
#define MAX_LAPS 5

char screen[HEIGHT][WIDTH + 1];

#ifndef _WIN32
static struct termios oldt, newt;
void init_termios() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}
void reset_termios() { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); }
int kbhit() {
    struct timeval tv = {0, 0};
    fd_set rdfs;
    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);
    select(1, &rdfs, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &rdfs);
}
#endif

struct Nitro {
    double x, y;
    int active;
    double respawn_time;
};

struct Car {
    double x, y;
    double vx, vy;
    double angle;
    int laps;
    int nitro;
    int color;
    char icon[8][4];
    const char* name;
};

struct Car cars[NUM_CARS];
struct Nitro nitros[NUM_NITROS];
int winner = -1;

void hide_cursor() { printf("\033[?25l"); }
void show_cursor() { printf("\033[?25h"); }

void clear_screen() { printf("\033[2J\033[H"); }

void build_track() {
    for (int y = 0; y < HEIGHT; y++)
        memset(screen[y], '.', WIDTH);

    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    double outer_a = 52.0, outer_b = 16.0;
    double inner_a = 32.0, inner_b = 9.0;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double dx = x - cx;
            double dy = y - cy;
            double od = dx*dx/(outer_a*outer_a) + dy*dy/(outer_b*outer_b);
            double id = dx*dx/(inner_a*inner_a) + dy*dy/(inner_b*inner_b);
            screen[y][x] = (od > 1.0 || id < 1.0) ? '#' : ' ';
        }
        screen[y][WIDTH] = '\0';
    }
}

void init_game() {
    build_track();

    const char* names[3] = {"PLAYER", "GREEN", "BLUE"};
    int colors[3] = {31, 32, 34};  // red, green, blue

    for (int i = 0; i < NUM_CARS; i++) {
        cars[i].x = 60 + i * 12;
        cars[i].y = HEIGHT - 8;
        cars[i].vx = cars[i].vy = 0;
        cars[i].angle = -M_PI / 2.0;  // facing up
        cars[i].laps = 0;
        cars[i].nitro = 50;
        cars[i].color = colors[i];
        cars[i].name = names[i];

        const char* icons[8] = {"↑", "↗", "→", "↘", "↓", "↙", "←", "↖"};
        for (int d = 0; d < 8; d++) strcpy(cars[i].icon[d], icons[d]);
    }

    double nitro_pos[NUM_NITROS][2] = {
        {45, 10}, {75, 10}, {30, 25}, {90, 25},
        {45, 32}, {75, 32}, {60, 18}, {60, 28}
    };
    for (int i = 0; i < NUM_NITROS; i++) {
        nitros[i].x = nitro_pos[i][0];
        nitros[i].y = nitro_pos[i][1];
        nitros[i].active = 1;
        nitros[i].respawn_time = 0;
    }
}

int is_wall(double x, double y) {
    int ix = (int)(x + 0.5);
    int iy = (int)(y + 0.5);
    if (ix < 0 || ix >= WIDTH || iy < 0 || iy >= HEIGHT) return 1;
    return screen[iy][ix] == '#';
}

void update_car(int i, double dt, int up, int down, int left, int right, int boost) {
    struct Car* c = &cars[i];
    double accel = (i == 0) ? 90.0 : 85.0 + i * 5.0;  // AI slightly different speeds
    double grip = (i == 0) ? 0.22 : 0.18;                    // player has best grip

    if (up || (i != 0)) {  // AI always accelerates
        c->vx += cos(c->angle) * accel * dt;
        c->vy += sin(c->angle) * accel * dt;
    }
    if (down) {
        c->vx -= cos(c->angle) * accel * 0.6 * dt;
        c->vy -= sin(c->angle) * accel * 0.6 * dt;
    }
    if (boost && c->nitro > 0) {
        c->vx += cos(c->angle) * 300 * dt;
        c->vy += sin(c->angle) * 300 * dt;
        c->nitro -= (int)(80 * dt);
        if (c->nitro < 0) c->nitro = 0;
    }

    if (left)  c->angle -= 4.8 * dt;
    if (right) c->angle += 4.8 * dt;

    // drag
    c->vx *= 0.982;
    c->vy *= 0.982;

    // grip - pull facing angle toward movement direction
    double speed = sqrt(c->vx*c->vx + c->vy*c->vy);
    if (speed > 0.1) {
        double move_angle = atan2(c->vy, c->vx);
        double diff = move_angle - c->angle;
        while (diff > M_PI) diff -= 2*M_PI;
        while (diff < -M_PI) diff += 2*M_PI;
        c->angle += diff * grip;
    }

    double old_y = c->y;
    c->x += c->vx * dt;
    c->y += c->vy * dt;

    // wall collision - elastic bounce
    if (is_wall(c->x, c->y) ||
        is_wall(c->x + 2, c->y) ||
        is_wall(c->x - 2, c->y) ||
        is_wall(c->x, c->y + 2) ||
        is_wall(c->x, c->y - 2)) {
        c->x -= c->vx * dt * 1.1;
        c->y -= c->vy * dt * 1.1;
        c->vx = -c->vx * 0.65;
        c->vy = -c->vy * 0.65;
    }

    // lap detection (crossing bottom straight upward)
    if (old_y > HEIGHT - 9 && c->y <= HEIGHT - 9 && c->vy < -5) {
        c->laps++;
        if (c->laps >= MAX_LAPS) winner = i;
    }

    // nitro pickup
    for (int n = 0; n < NUM_NITROS; n++) {
        if (nitros[n].active &&
            fabs(c->x - nitros[n].x) < 3 && fabs(c->y - nitros[n].y) < 3) {
            c->nitro += 80;
            nitros[n].active = 0;
            nitros[n].respawn_time = 8.0;  // respawn after 8 seconds
        }
    }
}

void ai_control(int i, double dt) {
    struct Car* c = &cars[i];
    int up = 1, down = 0, left = 0, right = 0, boost = 0;

    // simple look-ahead AI
    double ahead = 10.0 + sqrt(c->vx*c->vx + c->vy*c->vy) * 0.6;
    int lx = (int)(c->x + cos(c->angle) * ahead);
    int ly = (int)(c->y + sin(c->angle) * ahead);

    if (is_wall(lx, ly)) {
        // try turning left or right
        double la = c->angle - 1.2;
        double ra = c->angle + 1.2;
        int llx = (int)(c->x + cos(la) * ahead);
        int lly = (int)(c->y + sin(la) * ahead);
        int rlx = (int)(c->x + cos(ra) * ahead);
        int rly = (int)(c->y + sin(ra) * ahead);

        int left_wall  = is_wall(llx, lly);
        int right_wall = is_wall(rlx, rly);

        if (!left_wall && right_wall) left = 1;
        else if (!right_wall && left_wall) right = 1;
        else { left = 1; right = 1; }  // panic spin
    }

    if (c->nitro > 30 && rand() % 100 < 15) boost = 1;  // AI uses nitro occasionally

    update_car(i, dt, up, down, left, right, boost);
}

void render() {
    // copy base track
    static char base[HEIGHT][WIDTH + 1];
    static int first = 1;
    if (first) { memcpy(base, screen, sizeof(screen)); first = 0; }
    memcpy(screen, base, sizeof(screen));

    // place active nitros
    for (int i = 0; i < NUM_NITROS; i++) {
        if (nitros[i].active) {
            int x = (int)nitros[i].x;
            int y = (int)nitros[i].y;
            if (x > 0 && x < WIDTH-1 && y > 0 && y < HEIGHT-1)
                screen[y][x] = 'N';
        }
    }

    // draw cars
    for (int i = 0; i < NUM_CARS; i++) {
        int x = (int)(cars[i].x + 0.5);
        int y = (int)(cars[i].y + 0.5);
        if (x > 1 && x < WIDTH-2 && y > 1 && y < HEIGHT-2) {
            double a = cars[i].angle;
            if (a < 0) a += 2 * M_PI;
            int dir = (int)((a + M_PI/8) * 4 / M_PI) % 8;
            screen[y][x] = cars[i].icon[dir][0];
            // color the truck
            printf("\033[%d;%dH\033[%dm%s\033[0m", y+1, x+1, cars[i].color, cars[i].icon[dir]);
        }
    }

    // HUD
    printf("\033[1;1H");
    printf(" SUPER OFF ROAD CLONE    LAPS TO WIN: %d                  \n", MAX_LAPS);
    for (int i = 0; i < NUM_CARS; i++) {
        printf("\033[%d;1H\033[%dm%s\033[0m  Laps: %d  Nitro: %d   ", i+3, cars[i].color, cars[i].name, cars[i].laps, cars[i].nitro);
    }
    printf("\033[%d;1H", HEIGHT);
    fflush(stdout);
}

int main() {
    srand(time(NULL));
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // UTF-8 support
#else
    init_termios();
    atexit(reset_termios);
#endif
    hide_cursor();
    atexit(show_cursor);

    init_game();
    clear_screen();

    clock_t last = clock();

    while (winner == -1) {
        clock_t now = clock();
        double dt = (now - last) / (double)CLOCKS_PER_SEC;
        if (dt < 0.01) {
#ifdef _WIN32
            Sleep(5);
#else
            usleep(5000);
#endif
            continue;
        }
        last = now;

        // input
        int up = 0, down = 0, left = 0, right = 0, boost = 0, quit = 0;

#ifdef _WIN32
        up    = (GetAsyncKeyState(VK_UP)    || GetAsyncKeyState('W')) & 0x8000;
        down  = (GetAsyncKeyState(VK_DOWN)  || GetAsyncKeyState('S')) & 0x8000;
        left  = (GetAsyncKeyState(VK_LEFT)  || GetAsyncKeyState('A')) & 0x8000;
        right = (GetAsyncKeyState(VK_RIGHT) || GetAsyncKeyState('D')) & 0x8000;
        boost = GetAsyncKeyState(VK_SPACE) & 0x8000;
        quit  = GetAsyncKeyState(VK_ESCAPE) & 0x8000;
#else
        while (kbhit()) {
            char k = getchar();
            if (k == 'w' || k == 'W') up = 1;
            if (k == 's' || k == 'S') down = 1;
            if (k == 'a' || k == 'A') left = 1;
            if (k == 'd' || k == 'D') right = 1;
            if (k == ' ') boost = 1;
            if (k == 27 || k == 'q') quit = 1;
        }
#endif

        if (quit) break;

        update_car(0, dt, up, down, left, right, boost);
        for (int i = 1; i < NUM_CARS; i++) ai_control(i, dt);

        // respawn nitros
        for (int i = 0; i < NUM_NITROS; i++) {
            if (!nitros[i].active) {
                nitros[i].respawn_time -= dt;
                if (nitros[i].respawn_time <= 0) nitros[i].active = 1;
            }
        }

        render();
    }

    clear_screen();
    if (winner != -1) {
        printf("\n\n\n\t\t*** %s WINS THE RACE! ***\n\n", cars[winner].name);
    } else {
        printf("\n\n\n\t\tRace aborted.\n\n");
    }
    show_cursor();
    return 0;
}
