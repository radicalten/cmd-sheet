/*
 * Single-file top-down ASCII racing game.
 *
 * Platform: Unix-like terminal (Linux/macOS), ANSI escape capable.
 * Build: gcc -O2 -Wall -std=c99 -o ascii_racer ascii_racer.c
 *
 * Controls:
 *   W/A/S/D : move up/left/down/right
 *   Q       : quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>

/* --------- Configurable constants --------- */

#define MAP_W 40
#define MAP_H 17

#define HUD_ROWS 4          /* rows reserved for HUD at top */
#define MAX_PATH_LEN 512
#define MAX_ENEMIES 2
#define TARGET_FPS 25
#define LAPS_TO_WIN 3

/* --------- Basic types --------- */

typedef struct {
    int x, y;
} Point;

typedef struct {
    int x, y;
    int laps;
    int passed_checkpoint;
    int on_start;
    int finished;
    double finish_time;
} Player;

typedef struct {
    int path_index;
    int laps;
    int started;
    int finished;
    double finish_time;
    Point pos;
    int move_delay;   /* frames between moves */
    int move_counter;
    char symbol;
} EnemyCar;

/* --------- Globals for track and timing --------- */

static char base_map[MAP_H][MAP_W]; /* without dynamic entities */
static Point path[MAX_PATH_LEN];
static int path_len = 0;

static int start_x = 0, start_y = 0;
static int checkpoint_x = 0, checkpoint_y = 0;

/* Terminal state */
static struct termios orig_termios;
static int raw_mode_enabled = 0;

/* --------- Time helpers --------- */

static double get_time_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void format_time(double seconds, char *buf, size_t sz) {
    if (seconds < 0) seconds = 0;
    int minutes = (int)(seconds / 60.0);
    double rem = seconds - minutes * 60.0;
    int secs = (int)rem;
    int centi = (int)((rem - secs) * 100.0 + 0.5);
    snprintf(buf, sz, "%02d:%02d.%02d", minutes, secs, centi);
}

/* --------- Terminal raw mode / input --------- */

static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); /* no echo, no canonical mode */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    raw_mode_enabled = 1;
}

static int kbhit(void) {
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (r < 0) return 0;
    return FD_ISSET(STDIN_FILENO, &fds);
}

/* Returns -1 if no key, otherwise unsigned char in 0..255 */
static int read_key(void) {
    if (!kbhit()) return -1;
    unsigned char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) return -1;
    return (int)ch;
}

/* --------- Track generation --------- */

static void generate_track(void) {
    int x, y;

    /* Fill with spaces */
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            base_map[y][x] = ' ';
        }
    }

    /* Outer border walls */
    for (x = 0; x < MAP_W; x++) {
        base_map[0][x] = '#';
        base_map[MAP_H - 1][x] = '#';
    }
    for (y = 0; y < MAP_H; y++) {
        base_map[y][0] = '#';
        base_map[y][MAP_W - 1] = '#';
    }

    /* Inner border walls (one cell inside outer) */
    for (x = 1; x < MAP_W - 1; x++) {
        base_map[1][x] = '#';
        base_map[MAP_H - 2][x] = '#';
    }
    for (y = 1; y < MAP_H - 1; y++) {
        base_map[y][1] = '#';
        base_map[y][MAP_W - 2] = '#';
    }

    /* Interior filled with walls to keep a single ring track */
    int left = 2, right = MAP_W - 3;
    int top = 2, bottom = MAP_H - 3;

    for (y = top + 1; y < bottom; y++) {
        for (x = left + 1; x < right; x++) {
            base_map[y][x] = '#';
        }
    }

    /* Build the rectangular ring path at offset 2 */
    path_len = 0;

    /* Top side: left -> right */
    for (x = left; x <= right; x++) {
        base_map[top][x] = '.';
        if (path_len < MAX_PATH_LEN) {
            path[path_len].x = x;
            path[path_len].y = top;
            path_len++;
        }
    }

    /* Right side: top+1 -> bottom */
    for (y = top + 1; y <= bottom; y++) {
        base_map[y][right] = '.';
        if (path_len < MAX_PATH_LEN) {
            path[path_len].x = right;
            path[path_len].y = y;
            path_len++;
        }
    }

    /* Bottom side: right-1 -> left */
    for (x = right - 1; x >= left; x--) {
        base_map[bottom][x] = '.';
        if (path_len < MAX_PATH_LEN) {
            path[path_len].x = x;
            path[path_len].y = bottom;
            path_len++;
        }
    }

    /* Left side: bottom-1 -> top+1 */
    for (y = bottom - 1; y > top; y--) {
        base_map[y][left] = '.';
        if (path_len < MAX_PATH_LEN) {
            path[path_len].x = left;
            path[path_len].y = y;
            path_len++;
        }
    }

    if (path_len <= 0) {
        fprintf(stderr, "Path generation failed.\n");
        exit(1);
    }

    /* Define start line at first path cell, checkpoint roughly opposite */
    int start_index = 0;
    int checkpoint_index = path_len / 2;

    start_x = path[start_index].x;
    start_y = path[start_index].y;
    checkpoint_x = path[checkpoint_index].x;
    checkpoint_y = path[checkpoint_index].y;

    base_map[start_y][start_x] = 'S';
    base_map[checkpoint_y][checkpoint_x] = 'C';
}

/* --------- Drawing --------- */

static void draw_game(const Player *player,
                      const EnemyCar enemies[],
                      int num_enemies,
                      double elapsed) {
    int x, y, i;
    char buf[64];

    /* Move cursor to home (do not clear whole screen every frame) */
    printf("\033[H");

    /* HUD */
    printf("ASCII TOP-DOWN RACER\n");

    format_time(elapsed, buf, sizeof(buf));
    printf("Time: %s   Lap: %d/%d\n",
           buf, player->laps, LAPS_TO_WIN);

    printf("Enemies: ");
    for (i = 0; i < num_enemies; i++) {
        printf("%c:%d/%d ", enemies[i].symbol,
               enemies[i].laps, LAPS_TO_WIN);
    }
    printf("\n");

    printf("Controls: W/A/S/D to move, Q to quit\n");

    /* Build display buffer from base map and overlay entities */
    char disp[MAP_H][MAP_W];
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            disp[y][x] = base_map[y][x];
        }
    }

    /* Enemies */
    for (i = 0; i < num_enemies; i++) {
        if (!enemies[i].finished) {
            int ex = enemies[i].pos.x;
            int ey = enemies[i].pos.y;
            if (ex >= 0 && ex < MAP_W && ey >= 0 && ey < MAP_H) {
                disp[ey][ex] = enemies[i].symbol;
            }
        }
    }

    /* Player */
    if (!player->finished) {
        if (player->x >= 0 && player->x < MAP_W &&
            player->y >= 0 && player->y < MAP_H) {
            disp[player->y][player->x] = 'P';
        }
    }

    /* Print the map below the HUD */
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            putchar(disp[y][x]);
        }
        putchar('\n');
    }
    fflush(stdout);
}

/* --------- Game logic helpers --------- */

static void process_player_input(Player *player, int *running, int *player_quit) {
    int ch;

    while ((ch = read_key()) != -1) {
        if (ch == 'q' || ch == 'Q') {
            *running = 0;
            *player_quit = 1;
            break;
        }

        ch = toupper(ch);
        int dx = 0, dy = 0;

        if (ch == 'W') dy = -1;
        else if (ch == 'S') dy = 1;
        else if (ch == 'A') dx = -1;
        else if (ch == 'D') dx = 1;
        else continue; /* ignore other keys */

        int nx = player->x + dx;
        int ny = player->y + dy;

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) {
            continue;
        }

        char tile = base_map[ny][nx];
        if (tile == '.' || tile == 'S' || tile == 'C') {
            player->x = nx;
            player->y = ny;
        }
    }
}

static void update_player_lap(Player *player, double elapsed) {
    if (player->x == checkpoint_x && player->y == checkpoint_y) {
        player->passed_checkpoint = 1;
    }

    if (player->x == start_x && player->y == start_y) {
        if (!player->on_start && player->passed_checkpoint) {
            player->laps++;
            player->passed_checkpoint = 0;
            if (!player->finished && player->laps >= LAPS_TO_WIN) {
                player->finished = 1;
                player->finish_time = elapsed;
            }
        }
        player->on_start = 1;
    } else {
        player->on_start = 0;
    }
}

static void update_enemies(EnemyCar enemies[], int num_enemies, double elapsed) {
    int i;
    for (i = 0; i < num_enemies; i++) {
        EnemyCar *e = &enemies[i];

        if (e->finished) continue;

        e->move_counter++;
        if (e->move_counter < e->move_delay) {
            continue;
        }
        e->move_counter = 0;

        /* Move along path */
        e->path_index = (e->path_index + 1) % path_len;
        e->pos = path[e->path_index];

        /* Lap counting: first time crossing index 0 just "start"; afterward laps */
        if (e->path_index == 0) {
            if (e->started) {
                e->laps++;
                if (!e->finished && e->laps >= LAPS_TO_WIN) {
                    e->finished = 1;
                    e->finish_time = elapsed;
                }
            } else {
                e->started = 1;
            }
        }
    }
}

/* --------- Main --------- */

int main(void) {
    enable_raw_mode();
    atexit(disable_raw_mode);

    /* Clear screen, move home, hide cursor */
    printf("\033[2J\033[H\033[?25l");
    fflush(stdout);

    generate_track();

    Player player;
    player.x = start_x;
    player.y = start_y;
    player.laps = 0;
    player.passed_checkpoint = 0;
    player.on_start = 1; /* we start on the start line */
    player.finished = 0;
    player.finish_time = -1.0;

    EnemyCar enemies[MAX_ENEMIES];
    int num_enemies = MAX_ENEMIES;

    /* Place enemies at different positions on the path */
    int i;
    for (i = 0; i < num_enemies; i++) {
        int idx = (i + 1) * path_len / (num_enemies + 1);
        enemies[i].path_index = idx % path_len;
        enemies[i].pos = path[enemies[i].path_index];
        enemies[i].laps = 0;
        enemies[i].started = 0;
        enemies[i].finished = 0;
        enemies[i].finish_time = -1.0;
        enemies[i].move_counter = 0;
        enemies[i].move_delay = 2 + i; /* slightly different speeds */
        enemies[i].symbol = '1' + i;
    }

    double start_time = get_time_sec();
    int running = 1;
    int player_quit = 0;

    int frame_time_us = 1000000 / TARGET_FPS;

    while (running) {
        double now = get_time_sec();
        double elapsed = now - start_time;

        process_player_input(&player, &running, &player_quit);
        if (!running) break;

        if (!player.finished) {
            update_player_lap(&player, elapsed);
        }

        update_enemies(enemies, num_enemies, elapsed);

        /* Check race end conditions */
        int any_enemy_finished = 0;
        double best_enemy_finish = -1.0;

        for (i = 0; i < num_enemies; i++) {
            if (enemies[i].finished) {
                any_enemy_finished = 1;
                if (best_enemy_finish < 0 ||
                    enemies[i].finish_time < best_enemy_finish) {
                    best_enemy_finish = enemies[i].finish_time;
                }
            }
        }

        int player_finished = player.finished;

        int game_over = 0;
        int player_won = 0;

        if (player_finished && !any_enemy_finished) {
            game_over = 1;
            player_won = 1;
        } else if (any_enemy_finished && !player_finished) {
            game_over = 1;
            player_won = 0;
        } else if (any_enemy_finished && player_finished) {
            game_over = 1;
            if (player.finish_time <= best_enemy_finish) {
                player_won = 1;
            } else {
                player_won = 0;
            }
        }

        draw_game(&player, enemies, num_enemies, elapsed);

        if (game_over) {
            running = 0;
            /* Store result flags for victory screen via locals */
            /* We'll reconstruct from player/enemy state after loop. */
            /* Break out after a short delay so last frame is visible. */
            usleep(300000);
            break;
        }

        usleep(frame_time_us);
    }

    /* Restore cursor and terminal */
    printf("\033[?25h");
    fflush(stdout);
    disable_raw_mode();

    /* Victory / end screen */
    printf("\033[2J\033[H");

    if (player_quit) {
        printf("You quit the race.\n");
        printf("Thanks for playing.\n");
        return 0;
    }

    /* Determine final result again for clarity */
    int any_enemy_finished = 0;
    double best_enemy_finish = -1.0;
    int best_enemy_index = -1;

    for (i = 0; i < num_enemies; i++) {
        if (enemies[i].finished) {
            any_enemy_finished = 1;
            if (best_enemy_finish < 0 ||
                enemies[i].finish_time < best_enemy_finish) {
                best_enemy_finish = enemies[i].finish_time;
                best_enemy_index = i;
            }
        }
    }

    int player_finished = player.finished;
    int player_won = 0;
    if (player_finished && !any_enemy_finished) {
        player_won = 1;
    } else if (!player_finished && any_enemy_finished) {
        player_won = 0;
    } else if (any_enemy_finished && player_finished) {
        if (player.finish_time <= best_enemy_finish) {
            player_won = 1;
        } else {
            player_won = 0;
        }
    } else {
        /* No one finished: treat as DNF */
        player_won = 0;
    }

    char time_buf[64];
    double final_time = player_finished ? player.finish_time : (get_time_sec() - start_time);
    format_time(final_time, time_buf, sizeof(time_buf));

    if (player_won) {
        printf("========================================\n");
        printf("               YOU WIN!                 \n");
        printf("========================================\n\n");
    } else {
        printf("========================================\n");
        printf("               YOU LOSE                 \n");
        printf("========================================\n\n");
    }

    printf("Your laps: %d/%d  Time: %s\n", player.laps, LAPS_TO_WIN, time_buf);

    for (i = 0; i < num_enemies; i++) {
        char e_time[64];
        double t = enemies[i].finished ? enemies[i].finish_time : final_time;
        format_time(t, e_time, sizeof(e_time));
        printf("Enemy %c: laps %d/%d  Time: %s%s\n",
               enemies[i].symbol,
               enemies[i].laps, LAPS_TO_WIN,
               e_time,
               enemies[i].finished ? "" : " (DNF)");
    }

    if (best_enemy_index >= 0) {
        printf("\nFastest enemy: %c\n", enemies[best_enemy_index].symbol);
    }

    printf("\nPress Enter to exit...");
    fflush(stdout);
    /* Wait for Enter */
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);

    return 0;
}
