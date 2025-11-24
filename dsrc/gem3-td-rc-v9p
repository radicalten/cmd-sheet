#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

/* --- Configuration --- */
#define TRACK_W 40
#define TRACK_H 20
#define FPS 30
#define TARGET_LAPS 3
#define PI 3.1415926535
#define DEG_TO_RAD (PI / 180.0)

/* --- ANSI Escape Codes --- */
#define CLEAR_SCREEN "\033[2J"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define RESET_COLOR "\033[0m"
#define COL_WALL    "\033[37;40m" // White on Black
#define COL_GRASS   "\033[32;42m" // Green on Green
#define COL_TRACK   "\033[30;47m" // Black on Gray
#define COL_PLAYER  "\033[34;47m" // Blue on Gray
#define COL_ENEMY   "\033[31;47m" // Red on Gray
#define COL_UI      "\033[37;44m" // White on Blue

/* --- Global State & Map --- */
// 0: Track, 1: Wall, 2: Finish Line
const char MAP_DATA[TRACK_H][TRACK_W + 1] = {
    "########################################",
    "#S                                     #",
    "# #################################### #",
    "# #                                  # #",
    "# # ############################## # # #",
    "# # #                            # # # #",
    "# # # ########################## # # # #",
    "# # # #                        # # # # #",
    "# # # # ###################### # # # # #",
    "# # # # #                    # # # # # #",
    "# # # # # #################### # # # # #",
    "# # # # #                      # # # # #",
    "# # # # ######################## # # # #",
    "# # #                            # # # #",
    "# # ############################## # # #",
    "# #                                  # #",
    "# #################################### #",
    "#                                      #",
    "#                                      #",
    "########################################"
};

// Waypoints for Enemy AI (approximate center of track loop)
typedef struct { float x, y; } Point;
Point waypoints[] = {
    {3, 1}, {36, 1}, {36, 3}, {5, 3}, {5, 5}, {34, 5}, {34, 7}, 
    {7, 7}, {7, 9}, {32, 9}, {32, 11}, {7, 11}, {7, 13}, {34, 13},
    {34, 15}, {5, 15}, {5, 17}, {37, 17}, {37, 18}, {2, 18}, {2, 2}
};
int num_waypoints = sizeof(waypoints) / sizeof(Point);

typedef struct {
    float x, y;
    float angle; // in degrees
    float speed;
    float max_speed;
    int lap;
    int passed_halfway;
    int old_draw_x, old_draw_y; // For clean updating
    char symbol;
    const char* color;
} Car;

Car player, enemy;
int game_running = 1;
int victory = 0;
int defeat = 0;
time_t start_time;
struct termios orig_termios;

/* --- System Functions --- */

// Configure terminal for raw input
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and canonical mode
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    // Non-blocking IO
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf(SHOW_CURSOR RESET_COLOR "\n");
}

// Move cursor to specific row/col (1-based)
void goto_xy(int x, int y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

/* --- Game Logic --- */

void init_game() {
    // Player setup
    player.x = 2.0; player.y = 1.5;
    player.angle = 0; player.speed = 0;
    player.max_speed = 0.8;
    player.lap = 0; player.passed_halfway = 0;
    player.old_draw_x = -1; player.old_draw_y = -1;
    player.symbol = 'P'; player.color = COL_PLAYER;

    // Enemy setup
    enemy.x = 2.0; enemy.y = 1.5;
    enemy.angle = 0; enemy.speed = 0.55; // Constant speed
    enemy.lap = 0; enemy.passed_halfway = 0;
    enemy.old_draw_x = -1; enemy.old_draw_y = -1;
    enemy.symbol = 'E'; enemy.color = COL_ENEMY;

    start_time = time(NULL);
    
    // Draw Static Map ONCE
    printf(CLEAR_SCREEN HIDE_CURSOR);
    goto_xy(0, 0);
    
    for(int y=0; y<TRACK_H; y++) {
        for(int x=0; x<TRACK_W; x++) {
            char c = MAP_DATA[y][x];
            if (c == '#') printf(COL_WALL "#");
            else if (c == 'S') printf(COL_TRACK "S");
            else printf(COL_TRACK " ");
        }
        printf(RESET_COLOR "\n");
    }
    
    // Draw UI Box
    goto_xy(0, TRACK_H);
    printf(COL_UI " CONTROLS: W,A,S,D | Q to Quit           " RESET_COLOR);
    goto_xy(0, TRACK_H+1);
    printf(COL_UI " LAPS: 0/%d | TIME: 0s                   " RESET_COLOR, TARGET_LAPS);
}

int is_wall(float x, float y) {
    int ix = (int)x;
    int iy = (int)y;
    if (ix < 0 || ix >= TRACK_W || iy < 0 || iy >= TRACK_H) return 1;
    return MAP_DATA[iy][ix] == '#';
}

void update_physics(Car *c, int is_player) {
    float rad = c->angle * DEG_TO_RAD;
    float new_x = c->x + cos(rad) * c->speed;
    float new_y = c->y + sin(rad) * c->speed;

    // Collision detection (Simple bounce)
    if (!is_wall(new_x, new_y)) {
        c->x = new_x;
        c->y = new_y;
    } else {
        // Stop on collision
        if(is_player) c->speed = 0;
    }

    // Friction
    if (is_player) {
        if (c->speed > 0) c->speed -= 0.01;
        if (c->speed < 0) c->speed += 0.01;
        if (fabs(c->speed) < 0.01) c->speed = 0;
    }

    // Lap Logic
    // Checkpoint (approx middle of map logic) to prevent cheating
    if (c->y > TRACK_H - 5 && c->x > TRACK_W - 5) c->passed_halfway = 1;

    // Start line is at y=1 (approx) x=2. If we cross it going right...
    if (c->passed_halfway && c->y < 3 && c->x >= 2 && c->x < 4) {
        c->lap++;
        c->passed_halfway = 0;
    }
}

// Simple Enemy AI: Seek current waypoint
void update_enemy_ai() {
    static int wp_idx = 0;
    Point target = waypoints[wp_idx];

    // Distance to waypoint
    float dx = target.x - enemy.x;
    float dy = target.y - enemy.y;
    float dist = sqrt(dx*dx + dy*dy);

    // Angle to waypoint
    float target_angle = atan2(dy, dx) * (180.0 / PI);
    
    // Normalize angles
    float diff = target_angle - enemy.angle;
    while (diff <= -180) diff += 360;
    while (diff > 180) diff -= 360;

    // Turn towards target
    if (diff > 5) enemy.angle += 5;
    else if (diff < -5) enemy.angle -= 5;
    else enemy.angle = target_angle;

    // Advance waypoint if close
    if (dist < 1.5) {
        wp_idx++;
        if (wp_idx >= num_waypoints) wp_idx = 0;
    }
    
    update_physics(&enemy, 0);
}

// Update UI and Entities (In-place rendering)
void draw_frame() {
    // 1. Erase old positions (Draw track over them)
    Car *cars[] = {&player, &enemy};
    for(int i=0; i<2; i++) {
        if (cars[i]->old_draw_x != -1) {
            goto_xy(cars[i]->old_draw_x, cars[i]->old_draw_y);
            // Check if it was start line, otherwise track
            if (MAP_DATA[cars[i]->old_draw_y][cars[i]->old_draw_x] == 'S')
                 printf(COL_TRACK "S");
            else 
                 printf(COL_TRACK " ");
        }
    }

    // 2. Draw new positions
    for(int i=0; i<2; i++) {
        int ix = (int)cars[i]->x;
        int iy = (int)cars[i]->y;
        
        // Don't draw if inside wall (clipping fix)
        if(MAP_DATA[iy][ix] != '#') {
            goto_xy(ix, iy);
            printf("%s%c" RESET_COLOR, cars[i]->color, cars[i]->symbol);
            cars[i]->old_draw_x = ix;
            cars[i]->old_draw_y = iy;
        }
    }

    // 3. Update UI Stats
    goto_xy(0, TRACK_H+1);
    int t = (int)(time(NULL) - start_time);
    printf(COL_UI " LAPS: %d/%d | TIME: %ds | ENEMY: %d/%d  " RESET_COLOR, 
           player.lap, TARGET_LAPS, t, enemy.lap, TARGET_LAPS);

    fflush(stdout);
}

int main() {
    enable_raw_mode();
    init_game();

    char c;
    while (game_running) {
        // Input Handling
        while(read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q') game_running = 0;
            if (c == 'w') player.speed += 0.1;
            if (c == 's') player.speed -= 0.1;
            if (c == 'a') player.angle -= 15;
            if (c == 'd') player.angle += 15;
        }

        // Clamp Speed
        if (player.speed > player.max_speed) player.speed = player.max_speed;
        if (player.speed < -0.3) player.speed = -0.3;

        update_physics(&player, 1);
        update_enemy_ai();
        draw_frame();

        // Win/Loss Check
        if (player.lap >= TARGET_LAPS) {
            victory = 1;
            game_running = 0;
        }
        if (enemy.lap >= TARGET_LAPS) {
            defeat = 1;
            game_running = 0;
        }

        usleep(1000000 / FPS);
    }

    disable_raw_mode();
    printf(CLEAR_SCREEN);
    
    if (victory) {
        printf("\n\n\033[1;32m   VICTORY! YOU WON THE RACE! \033[0m\n\n");
    } else if (defeat) {
        printf("\n\n\033[1;31m   DEFEAT! THE ENEMY WAS FASTER. \033[0m\n\n");
    } else {
        printf("Game Quit.\n");
    }

    return 0;
}
