#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#define HEIGHT 25
#define WIDTH  50
#define NUM_CARS 4

struct termios oldt;

void restore_terminal() {
    tcsetattr(0, TCSANOW, &oldt);
}

struct Car {
    float x, y;
    float angle;
    float speed;      // forward speed
    float sideslip;   // lateral velocity
    char sym;
    const char *color;
    int lap;
    int wp_idx;
    int wrenches;
};

struct Car cars[NUM_CARS];
char map[HEIGHT][WIDTH+1];

const char *base_map[HEIGHT] = {
    "##################################################",
    "#*               ************************       *#",
    "# ####   ####################################   #",
    "# #  #   #                                 #   #",
    "# #  #### #   ########################### #   #",
    "# #       #   #                         # #   #",
    "# #  ######   #   ##################### #  #   #",
    "# #  #        #   #                   #    #   #",
    "# #  #  ###### #  #  ################# #####   #",
    "# #  #  #      #  #  #               #         #",
    "# #  #  #  ###### #  #  ############ # ####### #",
    "# #  #  #         #  #  #            # #       #",
    "# #  #  ######### #  #  #  ######### # # #### #",
    "# #  #            #  #  #            # # #  # #",
    "# #  ############## #  #  ############ # #  # #",
    "# #                 #  #                # #  # #",
    "# #  ################ #  ################ #  # #",
    "# #                   #                  #  # #",
    "# ####  ##############################  ##  ###",
    "#    #  #                            #  #     #",
    "#    #  #################################     *#",
    "#    #                                       * #",
    "#    ############################################",
    "#                                                #",
    "##################################################"
};

typedef struct { float x, y; } Vec2;

Vec2 waypoints[] = {
    {25,23},{30,23},{35,23},{40,21},{43,18},{43,15},{41,12},{37,9},{32,8},{25,8},
    {18,9},{14,12},{12,15},{12,18},{15,21},{20,23},{25,23},{30,22},{35,20},{39,18},
    {41,15},{40,11},{36,8},{30,8},{22,9},{17,12},{14,16},{14,20},{18,23},{24,23}
};
int n_wp = sizeof(waypoints)/sizeof(waypoints[0]);

void init() {
    tcgetattr(0, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &newt);
    atexit(restore_terminal);

    for(int y=0; y<HEIGHT; y++) strcpy(map[y], base_map[y]);

    // Player (red '1') starts at bottom center facing right
    cars[0] = (struct Car){25, 23, 0, 0, 0, '1', "\033[91m", 0, 0, 0};
    // AI drones (blue, green, yellow)
    cars[1] = (struct Car){24, 23.5, 0, 0, 0, '2', "\033[94m", 0, 5, 0};
    cars[2] = (struct Car){26, 23.5, 0, 0, 0, '3', "\033[92m", 0, 10, 0};
    cars[3] = (struct Car){25, 24.0, 0, 0, 0, '4', "\033[93m", 0, 15, 0};
}

int main() {
    init();

    while(1) {
        // === INPUT (non-blocking, works with key repeat) ===
        int left=0, right=0, gas=0, brake=0, quit=0;
        char buf[32];
        ssize_t n = read(0, buf, sizeof(buf));
        for(ssize_t i=0; i<n; i++) {
            if(buf[i]=='q' || buf[i]==27) quit = 1; // ESC or q
            if(buf[i]=='\x1b' && i+2<n && buf[i+1]=='[') {
                switch(buf[i+2]) {
                    case 'A': gas=1; break;
                    case 'B': brake=1; break;
                    case 'C': right=1; break;
                    case 'D': left=1; break;
                }
                i+=2;
            }
        }
        if(quit) break;

        // === UPDATE ALL CARS ===
        for(int i=0; i<NUM_CARS; i++) {
            struct Car *c = &cars[i];
            int player = (i==0);

            // ----- controls -----
            int c_gas   = player ? gas   : 1;
            int c_brake = player ? brake : 0;
            int c_left  = player ? left  : 0;
            int c_right = player ? right : 0;

            // ----- AI steering (waypoint pursuit) -----
            if(!player) {
                int look = 4 + (int)(fabs(c->speed)/4);
                int tid = (c->wp_idx + look) % n_wp;
                float dx = waypoints[tid].x - c->x;
                float dy = waypoints[tid].y - c->y;
                float desired = atan2f(dy, dx);
                float diff = desired - c->angle;
                while(diff >  M_PI) diff -= 2*M_PI;
                while(diff < -M_PI) diff += 2*M_PI;
                c->angle += diff * 0.18f; // aggressive steering

                // advance waypoint when close
                dx = c->x - waypoints[c->wp_idx].x;
                dy = c->y - waypoints[c->wp_idx].y;
                if(dx*dx + dy*dy < 30) c->wp_idx = (c->wp_idx + 1) % n_wp;
            }

            // ----- physics (authentic Super Sprint feel) -----
            int ix = (int)(c->x + 0.5f);
            int iy = (int)(c->y + 0.5f);
            int on_road = (ix>=0 && ix<WIDTH && iy>=0 && iy<HEIGHT && map[iy][ix]==' ');
            int wall    = (ix<0 || ix>=WIDTH || iy<0 || iy>=HEIGHT || map[iy][ix]=='#');

            if(wall) {
                c->speed = c->sideslip = 0;           // crash → full stop + "explosion" feel
            } else {
                // acceleration / braking
                if(c_gas)   c->speed += 0.45f;
                if(c_brake) c->speed -= 0.90f;

                // steering sharper at low speed (exactly like the arcade)
                float steer_power = 0.13f / (fabs(c->speed)*0.07f + 1.0f);
                if(c_left)  c->angle -= steer_power;
                if(c_right) c->angle += steer_power;

                // drag (heavier off-road)
                c->speed *= on_road ? 0.990f : 0.960f;

                // grip (high on road → kills sideways slide, low off-road → huge drifting)
                float grip = on_road ? 0.25f : 0.04f;
                c->sideslip *= (1.0f - grip);

                // wrench collection & upgrade (every 3 wrenches = big boost)
                if(map[iy][ix]=='*') {
                    map[iy][ix] = ' ';
                    if(player) {
                        c->wrenches++;
                        if(c->wrenches % 3 == 0) {
                            // upgrade just like real Super Sprint!
                            // (you'll feel each one instantly)
                        }
                    }
                }
            }

            // max speed increases with upgrades
            float max_spd = 13.0f + (c->wrenches/3)*4.0f;
            if(c->speed > max_spd) c->speed = max_spd;

            // apply velocity
            float vx = c->speed * cosf(c->angle) - c->sideslip * sinf(c->angle);
            float vy = c->speed * sinf(c->angle) + c->sideslip * cosf(c->angle);
            c->x += vx * 0.03f;
            c->y += vy * 0.03f;

            // lap detection (crossing bottom finish line going roughly rightward)
            static float prev_y[NUM_CARS];
            if(prev_y[i] > 22.0f && c->y <= 22.0f && c->x > 15 && c->x < 35 && cosf(c->angle) > 0.3f)
                c->lap++;
            prev_y[i] = c->y;
        }

        // === RENDER ===
        printf("\033[2J\033[H"); // clear + home
        for(int y=0; y<HEIGHT; y++) {
            for(int x=0; x<WIDTH; x++) {
                char t = map[y][x];
                int car_here = -1;
                for(int i=0; i<NUM_CARS; i++) {
                    if((int)(cars[i].x+0.5f)==x && (int)(cars[i].y+0.5f)==y)
                        car_here = i;
                }
                if(car_here != -1) {
                    printf("%s%c\033[0m", cars[car_here].color, cars[car_here].sym);
                } else if(t=='#') {
                    printf("\033[97m#\033[0m");     // white walls
                } else if(t=='*') {
                    printf("\033[93m*\033[0m");     // yellow wrench
                } else if(t==' ') {
                    printf("\033[100m \033[0m");    // dark gray road
                } else {
                    printf("\033[42m.\033[0m");     // green grass
                }
            }
            printf("\n");
        }
        printf("\033[0mLap %d  Wrenches %d  Upgrades %d   (q or ESC to quit)\n",
               cars[0].lap, cars[0].wrenches, cars[0].wrenches/3);

        usleep(30000); // ~33 fps, feels exactly like the arcade
    }
    return 0;
}
