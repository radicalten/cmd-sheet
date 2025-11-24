#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define COLS 80          // road width
#define ROWS 35          // screen height (road + sky)

double pos = 0.0;        // distance along track
double speed = 0.0;      // current speed
double player_x = 0.0;   // lateral position (-50...50 approx)
int score = 0;

void enable_raw_mode() {
    struct termios raw;
    tcgetattr(0, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(0, TCSANOW, &raw);
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    enable_raw_mode();
    printf("\033[?25l\033[2J");     // hide cursor, clear screen

    while (1) {
        // ----- input -----
        int ch;
        while ((ch = getchar()) != EOF) {
            if (ch == 'a' || ch == 'A') player_x -= 1.5 + speed/30.0;  // harder at high speed
            if (ch == 'd' || ch == 'D') player_x += 1.5 + speed/30.0;
            if (ch == 'w' || ch == 'W') speed += 8.0;
            if (ch == 's' || ch == 'S') speed = fmax(0.0, speed - 20.0);
            if (ch == 'q' || ch == 'Q') goto end;
        }

        pos += speed / 20.0;
        score += (int)speed / 5;
        if (speed > 0) speed *= 0.99;   // slight drag

        // ----- track definition (curves + hills) -----
        double track_curve = sin(pos / 140.0) * 38.0;   // big lazy curves
        double hill_height = sin(pos / 90.0) * 12.0;    // up/down hills

        printf("\033[H");    // return to top-left

        int horizon = (ROWS/3) + (int)hill_height;

        // ----- render from bottom (near) to horizon (far) -----
        for (int y = ROWS-1; y > horizon; y--) {
            double perspective = (double)(ROWS - y) / (ROWS - horizon);   // 0 at horizon, ~1 at bottom
            double curve_here   = track_curve * perspective * perspective; // curves bend realistically
            double middle       = COLS/2 + curve_here + player_x * perspective * 1.8;

            double road_width   = 6 + perspective * (COLS/2.4);

            int left  = (int)(middle - road_width);
            int right = (int)(middle + road_width);

            for (int x = 0; x < COLS; x++) {
                if (x >= left && x <= right) {
                    // road surface with alternating stripes
                    if (((int)(pos*3) + x) % 26 < 13)
                        printf("░");
                    else
                        printf("▒");
                } else {
                    // grass/rumble
                    printf("%c", (x % 4 == 0) ? '▓' : ' ');
                }
            }
            printf("\n");
        }

        // ----- sky -----
        for (int y = horizon; y >= 0; y--) {
            for (int x = 0; x < COLS; x++) printf(" ");
            printf("\n");
        }

        // ----- draw your car (fixed at bottom center) -----
        int car_x = COLS / 2;
        printf("\033[%d;%dH  \033[91m███\033[0m  ", ROWS-4, car_x-4);
        printf("\033[%d;%dH\033[91m████████\033[0m", ROWS-5, car_x-4);
        printf("\033[%d;%dH \033[91m██\033[0m██\033[91m██\033[0m ", ROWS-6, car_x-4);

        // ----- off-road detection & slowdown -----
        double bottom_middle = COLS/2 + track_curve + player_x * 1.8;
        double bottom_width  = COLS/2.4;
        if (fabs(car_x - bottom_middle) > bottom_width + 6) {
            speed *= 0.6;
            printf("\033[%d;1H\033[91mOFF ROAD!!!\033[0m", ROWS+1);
        }

        // ----- HUD -----
        printf("\033[%d;1HSPEED: \033[93m%3.0f\033[0m    SCORE: \033[92m%d\033[0m    (A/D steer  W/S gas/brake  Q quit)", ROWS, speed, score);

        usleep(30000);   // ~33 fps
    }

end:
    printf("\033[?25h\033[2J");   // show cursor, clear
    printf("Game Over! Final score: %d\n", score);
    return 0;
}
