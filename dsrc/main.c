#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/select.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 30
#define LAPS_TO_WIN 3
#define FPS 30
#define FRAME_TIME (1000000 / FPS)

typedef struct {
    double x, y, vx, vy, angle;
    int laps, passed_checkpoint;
    struct timeval start_time;
    int finished;
} Car;

char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
char track[SCREEN_HEIGHT][SCREEN_WIDTH];
Car player;
struct termios old_term;

void init_terminal() {
    struct termios new_term;
    tcgetattr(0, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_term);
    printf("\033[?25l\033[2J\033[H");
}

void restore_terminal() {
    tcsetattr(0, TCSANOW, &old_term);
    printf("\033[?25h\033[2J\033[H");
}

void plot_thick(int x, int y, int thickness) {
    for (int dy = -thickness; dy <= thickness; dy++) {
        for (int dx = -thickness; dx <= thickness; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < SCREEN_WIDTH && ny >= 0 && ny < SCREEN_HEIGHT) {
                track[ny][nx] = '#';
            }
        }
    }
}

void draw_ellipse(int cx, int cy, int rx, int ry, int thickness) {
    for (int angle = 0; angle < 360; angle += 2) {
        double rad = angle * M_PI / 180.0;
        int x = cx + (int)(rx * cos(rad));
        int y = cy + (int)(ry * sin(rad));
        plot_thick(x, y, thickness);
    }
}

void init_track() {
    memset(track, ' ', sizeof(track));
    
    int cy = SCREEN_HEIGHT / 2;
    int cx1 = 25, cx2 = 55;
    int rx = 12, ry = 8;
    
    draw_ellipse(cx1, cy, rx, ry, 1);
    draw_ellipse(cx2, cy, rx, ry, 1);
    
    // Finish line
    for (int dy = -ry; dy <= ry; dy++) {
        int y = cy + dy;
        if (y >= 0 && y < SCREEN_HEIGHT) {
            track[y][cx1 - rx - 1] = 'F';
        }
    }
    
    // Checkpoint marker
    for (int dy = -ry; dy <= ry; dy++) {
        int y = cy + dy;
        if (y >= 0 && y < SCREEN_HEIGHT) {
            track[y][cx2 + rx + 1] = 'C';
        }
    }
}

void init_game() {
    player.x = 10.0;
    player.y = SCREEN_HEIGHT / 2.0;
    player.vx = player.vy = player.angle = 0.0;
    player.laps = player.passed_checkpoint = player.finished = 0;
    gettimeofday(&player.start_time, NULL);
}

int is_on_track(int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return 0;
    char c = track[y][x];
    return c == '#' || c == 'F' || c == 'C';
}

double get_elapsed_time() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - player.start_time.tv_sec) +
           (now.tv_usec - player.start_time.tv_usec) / 1000000.0;
}

void check_progress() {
    int ix = (int)player.x, iy = (int)player.y;
    if (ix < 0 || ix >= SCREEN_WIDTH || iy < 0 || iy >= SCREEN_HEIGHT) return;
    
    if (track[iy][ix] == 'C' && !player.passed_checkpoint) {
        player.passed_checkpoint = 1;
    }
    
    if (track[iy][ix] == 'F' && player.passed_checkpoint) {
        player.laps++;
        player.passed_checkpoint = 0;
        if (player.laps >= LAPS_TO_WIN) {
            player.finished = 1;
        }
    }
}

void update_car(char input) {
    double accel = 0.25, friction = 0.94, turn = 0.12;
    
    if (input == 'w' || input == 'W') {
        player.vx += cos(player.angle) * accel;
        player.vy += sin(player.angle) * accel;
    }
    if (input == 's' || input == 'S') {
        player.vx -= cos(player.angle) * accel * 0.6;
        player.vy -= sin(player.angle) * accel * 0.6;
    }
    if (input == 'a' || input == 'A') player.angle -= turn;
    if (input == 'd' || input == 'D') player.angle += turn;
    
    player.vx *= friction;
    player.vy *= friction;
    
    double speed = sqrt(player.vx * player.vx + player.vy * player.vy);
    if (speed > 1.8) {
        player.vx = (player.vx / speed) * 1.8;
        player.vy = (player.vy / speed) * 1.8;
    }
    
    double new_x = player.x + player.vx;
    double new_y = player.y + player.vy;
    
    if (is_on_track((int)new_x, (int)new_y)) {
        player.x = new_x;
        player.y = new_y;
        check_progress();
    } else {
        player.vx *= -0.3;
        player.vy *= -0.3;
    }
}

void draw_screen() {
    memcpy(screen, track, sizeof(screen));
    
    int car_x = (int)player.x, car_y = (int)player.y;
    if (car_x >= 0 && car_x < SCREEN_WIDTH && car_y >= 0 && car_y < SCREEN_HEIGHT) {
        char car_chars[] = ">\\|/";
        int dir = ((int)((player.angle + M_PI / 4) / (M_PI / 2)) + 4) % 4;
        screen[car_y][car_x] = car_chars[dir];
    }
    
    printf("\033[H");
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        fwrite(screen[y], 1, SCREEN_WIDTH, stdout);
        putchar('\n');
    }
    
    printf("\n╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ LAP: %d/%d │ TIME: %6.2fs │ CONTROLS: W/A/S/D - Drive │ Q - Quit      ║\n",
           player.laps, LAPS_TO_WIN, get_elapsed_time());
    printf("╚═══════════════════════════════════════════════════════════════════════════╝");
    fflush(stdout);
}

void draw_victory() {
    printf("\033[2J\033[H\n\n\n");
    printf("          ╔══════════════════════════════════════════════════╗\n");
    printf("          ║                                                  ║\n");
    printf("          ║              🏁 RACE COMPLETE! 🏁                ║\n");
    printf("          ║                                                  ║\n");
    printf("          ║         Final Time: %6.2f seconds              ║\n", get_elapsed_time());
    printf("          ║              Laps: %d/%d                          ║\n", LAPS_TO_WIN, LAPS_TO_WIN);
    printf("          ║                                                  ║\n");
    printf("          ╚══════════════════════════════════════════════════╝\n");
    printf("\n                    Press any key to exit...\n");
    fflush(stdout);
}

char get_key() {
    char c = 0;
    read(0, &c, 1);
    return c;
}

int main() {
    init_terminal();
    init_track();
    
