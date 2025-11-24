// offroad.c - Single-file terminal top-down racing game (no external deps)
// Inspired by Super Off Road / Super Sprint (one-screen, arcade-y).
// - Linux/macOS/Windows 10+ (VT console) supported.
// - Compile: 
//   Linux/macOS: gcc -O2 -std=c99 -o offroad offroad.c -lm
//   Windows MSVC: cl /O2 offroad.c
//   Windows MinGW: gcc -O2 -std=gnu99 -o offroad.exe offroad.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#if defined(_WIN32)
  #include <windows.h>
  #include <conio.h>
  #define usleep(x) Sleep((x)/1000)
#else
  #include <unistd.h>
  #include <termios.h>
  #include <fcntl.h>
  #include <sys/time.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Screen/track sizes
#define SCREEN_W 80
#define SCREEN_H 28
#define HUD_ROWS 2
#define TRACK_W SCREEN_W
#define TRACK_H (SCREEN_H - HUD_ROWS)

#define MAX_CARS 4
#define NUM_AI (MAX_CARS-1)
#define MAX_POINTS 20
#define MAX_ITEMS 16
#define FPS 60
#define DT (1.0f/FPS)
#define TARGET_LAPS 5

// ANSI helpers
#define CSI "\x1b["
#define ESC "\x1b"
static const char* CLR_RESET = CSI "0m";

typedef enum {
  COL_DEFAULT=0,
  COL_GRASS,
  COL_ROAD,
  COL_WALL,
  COL_OIL,
  COL_NITRO,
  COL_HUD,
  COL_P1,
  COL_AI1,
  COL_AI2,
  COL_AI3
} Color;

typedef struct {
  float x, y;
} Vec2;

typedef struct {
  float x, y, ang;
  float vx, vy;
  float steer;    // -1..1
  float throttle; // -1..1
  float last_x, last_y; // previous position (for checkpoints)
  int next_cp;    // next checkpoint index to hit
  int laps;
  float progress; // overall race progress
  int is_human;
  int nitro;      // number of nitros
  float nitro_time; // time left in active nitro burst
  int color;
  int id;         // 0=player, 1..3 ai
} Car;

typedef enum { ITEM_NITRO=1 } ItemType;

typedef struct {
  ItemType type;
  float x, y;
  int active;
  float respawn_timer;
} Item;

typedef struct {
  Vec2 p[MAX_POINTS];
  int n;
  int start_idx; // checkpoint index for start/finish line
  float road_halfw; // in tiles
} Path;

typedef struct {
  int up,down,left,right;
  int w,a,s,d;
  int nitro;
  int reset;
  int pause;
  int quit;
} Input;

// Globals for terminal handling
#if !defined(_WIN32)
static struct termios g_orig_term;
#endif
static int g_inited_console = 0;

// Time util
static double now_sec() {
#if defined(_WIN32)
  static LARGE_INTEGER freq;
  static int init=0;
  if(!init){ QueryPerformanceFrequency(&freq); init=1; }
  LARGE_INTEGER t; QueryPerformanceCounter(&t);
  return (double)t.QuadPart / (double)freq.QuadPart;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec/1000000.0;
#endif
}

// Terminal control
static void console_enter_alt() {
  printf(CSI "?1049h"); // alt screen
  printf(CSI "?25l");   // hide cursor
  fflush(stdout);
}
static void console_leave_alt() {
  printf(CSI "?25h");
  printf(CSI "?1049l");
  fflush(stdout);
}
static void console_clear() {
  printf(CSI "2J" CSI "H");
}
static void console_goto(int row, int col) {
  printf(CSI "%d;%dH", row, col);
}
static void console_color(Color c) {
  switch(c){
    case COL_GRASS: printf(CSI "38;5;34m"); break;    // green
    case COL_ROAD:  printf(CSI "38;5;245m"); break;   // gray
    case COL_WALL:  printf(CSI "38;5;250m"); break;   // light gray
    case COL_OIL:   printf(CSI "38;5;240m"); break;   // dark gray
    case COL_NITRO: printf(CSI "38;5;201m"); break;   // magenta
    case COL_HUD:   printf(CSI "38;5;228m"); break;   // yellow
    case COL_P1:    printf(CSI "38;5;196m"); break;   // red
    case COL_AI1:   printf(CSI "38;5;39m");  break;   // blue
    case COL_AI2:   printf(CSI "38;5;46m");  break;   // green
    case COL_AI3:   printf(CSI "38;5;226m"); break;   // yellow
    default:        printf(CLR_RESET); break;
  }
}

#if defined(_WIN32)
static DWORD g_old_mode_out=0, g_old_mode_in=0;
static HANDLE g_hOut=NULL, g_hIn=NULL;
#endif

static void console_init() {
  if (g_inited_console) return;
  g_inited_console = 1;

#if defined(_WIN32)
  g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  g_hIn  = GetStdHandle(STD_INPUT_HANDLE);
  DWORD modeOut=0, modeIn=0;
  GetConsoleMode(g_hOut, &g_old_mode_out);
  GetConsoleMode(g_hIn,  &g_old_mode_in);
  modeOut = g_old_mode_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(g_hOut, modeOut);
  // For input, we’ll leave cooked mode but use _kbhit/_getch
  // (no need to change console input modes).
#else
  struct termios raw;
  tcgetattr(STDIN_FILENO, &g_orig_term);
  raw = g_orig_term;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_oflag &= ~(OPOST);
  raw.c_cc[VMIN]  = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  // make stdin non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif
  console_enter_alt();
  console_clear();
}

static void console_shutdown() {
  if (!g_inited_console) return;
#if defined(_WIN32)
  if (g_hOut) SetConsoleMode(g_hOut, g_old_mode_out);
  if (g_hIn)  SetConsoleMode(g_hIn,  g_old_mode_in);
#else
  tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
#endif
  console_leave_alt();
  g_inited_console = 0;
}

// Input handling (arrow keys, WASD, space, r/p/q)
static int read_byte_nonblock() {
#if defined(_WIN32)
  if (_kbhit()) {
    return (unsigned char)_getch();
  }
  return -1;
#else
  unsigned char c;
  int n = (int)read(STDIN_FILENO, &c, 1);
  if (n <= 0) return -1;
  return (int)c;
#endif
}

static void poll_input(Input* in) {
  // clear transient
  in->nitro = in->reset = in->pause = in->quit = 0;
  // Maintain held-state via OS key repeat. We update pressed flags when bytes arrive.
  int b;
  while ((b = read_byte_nonblock()) != -1) {
#if defined(_WIN32)
    // Windows arrow keys come as 0 or 224, then code
    if (b == 0 || b == 224) {
      int c = read_byte_nonblock();
      if (c == -1) break;
      if (c == 72) in->up = 1;
      else if (c == 80) in->down = 1;
      else if (c == 75) in->left = 1;
      else if (c == 77) in->right = 1;
      continue;
    }
#endif
    if (b == 27) { // ESC or arrow sequence
#if !defined(_WIN32)
      int b1 = read_byte_nonblock();
      if (b1 == '[') {
        int b2 = read_byte_nonblock();
        if (b2 == 'A') in->up = 1;
        else if (b2 == 'B') in->down = 1;
        else if (b2 == 'C') in->right = 1;
        else if (b2 == 'D') in->left = 1;
      }
#endif
      // If raw ESC pressed, treat as quit
      // else ignore plain ESC.
      continue;
    }
    char c = (char)b;
    if (c == 'q' || c == 'Q') in->quit = 1;
    else if (c == 'p' || c == 'P') in->pause = 1;
    else if (c == 'r' || c == 'R') in->reset = 1;
    else if (c == ' ') in->nitro = 1;
    else if (c == 'w' || c == 'W') in->w = 1;
    else if (c == 's' || c == 'S') in->s = 1;
    else if (c == 'a' || c == 'A') in->a = 1;
    else if (c == 'd' || c == 'D') in->d = 1;
  }

  // decay held states slightly between repeats (so key repeat keeps them on)
  static double last_t = 0;
  static double decay_timer = 0;
  double t = now_sec();
  if (last_t == 0) last_t = t;
  decay_timer += (t - last_t);
  last_t = t;
  if (decay_timer >= 0.12) {
    // decay if no recent repeats
    in->up = in->down = in->left = in->right = 0;
    in->w = in->a = in->s = in->d = 0;
    decay_timer = 0;
  }
}

// Math helpers
static float clampf(float x, float a, float b){ return x < a ? a : (x > b ? b : x); }
static float wrap_angle(float a) {
  while (a > (float)M_PI) a -= (float)(2*M_PI);
  while (a < (float)-M_PI) a += (float)(2*M_PI);
  return a;
}
static float len2(Vec2 v){ return sqrtf(v.x*v.x + v.y*v.y); }
static float dot2(Vec2 a, Vec2 b){ return a.x*b.x + a.y*b.y; }
static Vec2 sub2(Vec2 a, Vec2 b){ Vec2 r={a.x-b.x, a.y-b.y}; return r; }
static Vec2 add2(Vec2 a, Vec2 b){ Vec2 r={a.x+b.x, a.y+b.y}; return r; }
static Vec2 mul2(Vec2 a, float s){ Vec2 r={a.x*s, a.y*s}; return r; }
static Vec2 norm2(Vec2 v){ float L = len2(v); if (L<1e-6f){Vec2 z={0,0};return z;} return mul2(v, 1.0f/L); }

static float dist_point_seg(Vec2 p, Vec2 a, Vec2 b) {
  Vec2 ab = sub2(b,a);
  float ab2 = dot2(ab,ab);
  if (ab2 <= 1e-6f) return len2(sub2(p,a));
  float t = dot2(sub2(p,a), ab) / ab2;
  t = clampf(t, 0.0f, 1.0f);
  Vec2 proj = add2(a, mul2(ab, t));
  return len2(sub2(p, proj));
}

// Track tiles
static char g_tile[TRACK_H][TRACK_W];
static Color g_color[TRACK_H][TRACK_W];

static Path g_path;
static Item g_items[MAX_ITEMS];
static int g_items_n = 0;

// Predefine racing line (closed loop)
static void init_path(Path* path) {
  // A twisty loop across 80x26 tiles
  // You can tweak these to reshape the track.
  Vec2 pts[] = {
    {8,5}, {20,3}, {38,3}, {58,5}, {70,8}, {74,12},
    {72,16}, {62,19}, {46,21}, {30,22}, {16,20}, {7,16},
    {5,12}, {7,8}
  };
  int n = (int)(sizeof(pts)/sizeof(pts[0]));
  path->n = n;
  for (int i=0;i<n;i++) path->p[i] = pts[i];
  path->start_idx = 10; // near bottom-left straight
  path->road_halfw = 4.2f;
}

// Build track from path - rasterize road as thick polyline
static void build_track() {
  // Fill with grass
  for (int y=0;y<TRACK_H;y++){
    for (int x=0;x<TRACK_W;x++){
      g_tile[y][x] = ' ';
      g_color[y][x] = COL_GRASS;
    }
  }
  // Border walls
  for (int x=0;x<TRACK_W;x++){
    g_tile[0][x] = '#'; g_color[0][x] = COL_WALL;
    g_tile[TRACK_H-1][x] = '#'; g_color[TRACK_H-1][x] = COL_WALL;
  }
  for (int y=0;y<TRACK_H;y++){
    g_tile[y][0] = '#'; g_color[y][0] = COL_WALL;
    g_tile[y][TRACK_W-1] = '#'; g_color[y][TRACK_W-1] = COL_WALL;
  }

  // Paint road ('.') by distance to segments
  for (int y=0;y<TRACK_H;y++){
    for (int x=0;x<TRACK_W;x++){
      Vec2 p = {x+0.5f, y+0.5f};
      float mind = 1e9f;
      for (int i=0;i<g_path.n;i++){
        Vec2 a = g_path.p[i];
        Vec2 b = g_path.p[(i+1)%g_path.n];
        float d = dist_point_seg(p,a,b);
        if (d < mind) mind = d;
      }
      if (mind <= g_path.road_halfw) {
        g_tile[y][x] = '.';
        g_color[y][x] = COL_ROAD;
      }
    }
  }

  // Place some internal walls/islands that intersect near the inside of corners
  // Only place walls on non-road tiles to create barriers
  for (int y=3;y<TRACK_H-3;y++){
    for (int x=3;x<TRACK_W-3;x++){
      // a few rectangular islands
      if ((x>25 && x<30 && y>6 && y<14) ||
          (x>50 && x<54 && y>15 && y<22) ||
          (x>66 && x<69 && y>8 && y<12)) {
        if (g_tile[y][x] != '.') {
          g_tile[y][x] = '#';
          g_color[y][x] = COL_WALL;
        }
      }
    }
  }

  // Start/finish line
  int si = g_path.start_idx;
  Vec2 A = g_path.p[si];
  Vec2 B = g_path.p[(si+1)%g_path.n];
  Vec2 dir = norm2(sub2(B,A));
  Vec2 nrm = (Vec2){-dir.y, dir.x};
  for (int k=- (int)(g_path.road_halfw*1.4f); k <= (int)(g_path.road_halfw*1.4f); k++) {
    Vec2 pos = add2(A, mul2(nrm, (float)k));
    int x = (int)pos.x;
    int y = (int)pos.y;
    if (x>=1 && x<TRACK_W-1 && y>=1 && y<TRACK_H-1) {
      g_tile[y][x] = '|'; 
      g_color[y][x] = COL_HUD;
    }
  }

  // Oil slicks 'o' placed on road at some corners
  int oils_placed = 0;
  for (int i=0;i<g_path.n && oils_placed<6;i+=2){
    Vec2 P = g_path.p[i];
    Vec2 Q = g_path.p[(i+1)%g_path.n];
    Vec2 d = norm2(sub2(Q,P));
    Vec2 n = (Vec2){-d.y, d.x};
    Vec2 pos = add2(P, add2(mul2(d, 3.0f), mul2(n, (i%4==0)? 2.0f : -2.0f)));
    int x = (int)(pos.x+0.5f), y = (int)(pos.y+0.5f);
    if (x>=1 && x<TRACK_W-1 && y>=1 && y<TRACK_H-1 && g_tile[y][x]=='.') {
      g_tile[y][x]='o'; g_color[y][x]=COL_OIL;
      oils_placed++;
    }
  }

  // Nitro pickups 'N'
  g_items_n = 0;
  for (int i=1;i<g_path.n && g_items_n<6;i+=2) {
    Vec2 P = g_path.p[i];
    Vec2 Q = g_path.p[(i+1)%g_path.n];
    Vec2 d = norm2(sub2(Q,P));
    Vec2 n = (Vec2){-d.y, d.x};
    Vec2 pos = add2(P, add2(mul2(d, 4.0f), mul2(n, (i%3==0)? -2.5f : 2.5f)));
    int x = (int)(pos.x+0.5f), y = (int)(pos.y+0.5f);
    if (x>=1 && x<TRACK_W-1 && y>=1 && y<TRACK_H-1 && g_tile[y][x]=='.') {
      g_items[g_items_n].type = ITEM_NITRO;
      g_items[g_items_n].x = pos.x;
      g_items[g_items_n].y = pos.y;
      g_items[g_items_n].active = 1;
      g_items[g_items_n].respawn_timer = 0;
      g_items_n++;
      g_tile[y][x] = 'N';
      g_color[y][x] = COL_NITRO;
    }
  }
}

// Surface properties from tile
typedef struct {
  float thrust;   // forward acceleration factor
  float roll;     // linear drag
  float grip;     // lateral grip (higher = less slide)
} Surf;

static Surf surf_from_tile(char t) {
  // Tuned for arcade feel
  Surf s;
  if (t=='o') { // oil
    s.thrust = 26.0f; s.roll = 2.2f; s.grip = 6.0f;
  } else if (t=='.' || t=='|' || t=='N') { // road / start / nitro tile
    s.thrust = 30.0f; s.roll = 3.0f; s.grip = 14.0f;
  } else if (t=='#') { // wall (not actually used for motion)
    s.thrust = 0.0f; s.roll = 100.0f; s.grip = 50.0f;
  } else { // grass/dirt
    s.thrust = 19.0f; s.roll = 5.0f; s.grip = 10.0f;
  }
  return s;
}

static char tile_at(float x, float y) {
  int ix = (int)floorf(x);
  int iy = (int)floorf(y);
  if (ix<0 || iy<0 || ix>=TRACK_W || iy>=TRACK_H) return '#';
  return g_tile[iy][ix];
}

// Cars
static Car g_cars[MAX_CARS];

// Spawn cars on/near start line in grid
static void spawn_cars() {
  int si = g_path.start_idx;
  Vec2 A = g_path.p[si];
  Vec2 B = g_path.p[(si+1)%g_path.n];
  Vec2 dir = norm2(sub2(B,A));
  Vec2 nrm = (Vec2){-dir.y, dir.x};
  // Positions: two rows of two
  Vec2 base = add2(A, mul2(dir, -3.0f)); // slightly behind start line
  Vec2 grid[4];
  grid[0] = add2(base, add2(mul2(nrm, -1.7f), mul2(dir, -0.0f)));
  grid[1] = add2(base, add2(mul2(nrm,  1.7f), mul2(dir, -0.0f)));
  grid[2] = add2(base, add2(mul2(nrm, -1.7f), mul2(dir, -2.4f)));
  grid[3] = add2(base, add2(mul2(nrm,  1.7f), mul2(dir, -2.4f)));

  for (int i=0;i<MAX_CARS;i++){
    g_cars[i].x = grid[i].x;
    g_cars[i].y = grid[i].y;
    g_cars[i].ang = atan2f(dir.y, dir.x);
    g_cars[i].vx = g_cars[i].vy = 0;
    g_cars[i].steer = 0;
    g_cars[i].throttle = 0;
    g_cars[i].next_cp = (g_path.start_idx + 1) % g_path.n;
    g_cars[i].laps = 0;
    g_cars[i].progress = 0;
    g_cars[i].nitro = (i==0)? 1 : 0; // player starts with 1 nitro
    g_cars[i].nitro_time = 0;
    g_cars[i].last_x = g_cars[i].x;
    g_cars[i].last_y = g_cars[i].y;
    g_cars[i].id = i;
    g_cars[i].is_human = (i==0);
    g_cars[i].color = i==0 ? COL_P1 : (i==1?COL_AI1 : (i==2?COL_AI2:COL_AI3));
  }
}

// Checkpoint logic: cross perpendicular line at point path.p[i]
static void update_checkpoints(Car* c) {
  int i = c->next_cp;
  Vec2 P = g_path.p[i];
  Vec2 Pn = g_path.p[(i+1)%g_path.n];
  Vec2 d = norm2(sub2(Pn, P));
  // Half-plane test across line perpendicular to d at P:
  // Crossing occurs when dot((pos-P), d) changes from <0 to >=0 and close to the line width.
  Vec2 pos0 = (Vec2){c->last_x, c->last_y};
  Vec2 pos1 = (Vec2){c->x, c->y};
  float s0 = dot2(sub2(pos0,P), d);
  float s1 = dot2(sub2(pos1,P), d);

  // Also require car to be within track width
  Vec2 n = (Vec2){-d.y, d.x};
  float t = fabsf(dot2(sub2(pos1,P), n));
  float pass_width = g_path.road_halfw * 1.25f;

  if (s0 < 0 && s1 >= 0 && t <= pass_width) {
    c->next_cp = (c->next_cp + 1) % g_path.n;
    if (c->next_cp == (g_path.start_idx + 1) % g_path.n) {
      // Completed a lap
      c->laps++;
    }
  }

  // progress metric (laps * N + cp index + fractional along current segment)
  int prev_cp = (c->next_cp - 1 + g_path.n) % g_path.n;
  Vec2 A = g_path.p[prev_cp];
  Vec2 B = g_path.p[(prev_cp+1)%g_path.n];
  Vec2 AB = sub2(B,A);
  float ab2 = dot2(AB,AB);
  float frac = 0.0f;
  if (ab2 > 1e-5f) {
    frac = clampf(dot2(sub2((Vec2){c->x, c->y}, A), AB) / ab2, 0.0f, 1.0f);
  }
  c->progress = c->laps * (float)g_path.n + (float)prev_cp + frac;
}

// Physics update
static void physics_car(Car* c, float dt) {
  c->last_x = c->x; c->last_y = c->y;

  // Input already converted to c->steer and c->throttle by control functions

  // surface under car
  char T = tile_at(c->x, c->y);
  Surf s = surf_from_tile(T);

  float thrust = s.thrust;
  float roll = s.roll;
  float grip = s.grip;

  // Nitro effect
  if (c->nitro_time > 0) {
    thrust *= 1.6f;
    grip   *= 1.1f;
    roll   *= 0.9f;
    c->nitro_time -= dt;
    if (c->nitro_time < 0) c->nitro_time = 0;
  }

  // Velocity in car frame
  float ca = cosf(c->ang), sa = sinf(c->ang);
  float vf =  ca * c->vx + sa * c->vy;   // forward
  float vs = -sa * c->vx + ca * c->vy;   // sideways

  // Longitudinal
  float acc = c->throttle * thrust - roll * vf;
  vf += acc * dt;

  // Lateral grip (tends vs -> 0)
  float side_decay = grip * dt;
  if (side_decay > 1.0f) side_decay = 1.0f;
  vs = vs * (1.0f - side_decay);

  // Recompose velocity
  c->vx =  ca * vf - sa * vs;
  c->vy =  sa * vf + ca * vs;

  // Turn rate depends on speed and grip
  float speed = sqrtf(c->vx*c->vx + c->vy*c->vy);
  float turn_gain = (0.7f + 0.5f * clampf(fabsf(vf)/12.0f, 0.0f, 1.0f)) * (grip/14.0f);
  float turn_rate = 2.8f * turn_gain; // rad/s at high speed
  c->ang = wrap_angle(c->ang + c->steer * turn_rate * dt);

  // Integrate position
  float nx = c->x + c->vx * dt;
  float ny = c->y + c->vy * dt;

  // Collide with walls: if landing in '#', bounce/dampen
  char Nt = tile_at(nx, ny);
  if (Nt == '#') {
    // simple bounce: reflect velocity and dampen
    // Try separating along normal by testing neighbors
    int ix = (int)floorf(nx), iy = (int)floorf(ny);
    if (ix<0||iy<0||ix>=TRACK_W||iy>=TRACK_H) {
      c->vx *= -0.3f; c->vy *= -0.3f; // bounce back
    } else {
      // push back to last valid pos
      nx = c->x; ny = c->y;
      // dampen
      c->vx *= -0.25f; c->vy *= -0.25f;
    }
  }
  c->x = nx; c->y = ny;

  // basic floor friction when very slow
  if (speed < 0.02f) { c->vx = c->vy = 0; }

  // pickup nitros
  for (int i=0;i<g_items_n;i++){
    if (!g_items[i].active) continue;
    if (g_items[i].type == ITEM_NITRO) {
      float dx = c->x - g_items[i].x;
      float dy = c->y - g_items[i].y;
      if (dx*dx + dy*dy < 0.8f*0.8f) {
        c->nitro++;
        g_items[i].active = 0;
        g_items[i].respawn_timer = 8.0f; // respawn in 8 seconds
      }
    }
  }

  update_checkpoints(c);
}

// AI driver: pure pursuit + simple throttle logic
static void ai_control(Car* c, float dt, int ai_index) {
  // look ahead along path
  int i = c->next_cp;
  float look = 5.5f + 0.8f * ai_index; // different lookahead for variety
  // Walk along path to find look point
  Vec2 pos = {c->x, c->y};
  Vec2 target = g_path.p[i];
  float remaining = look;
  int idx = i;
  while (remaining > 0.0f) {
    Vec2 A = g_path.p[idx];
    Vec2 B = g_path.p[(idx+1)%g_path.n];
    Vec2 AB = sub2(B,A);
    float L = len2(AB);
    if (L >= remaining) {
      Vec2 dir = mul2(AB, remaining/L);
      target = add2(A, dir);
      break;
    } else {
      remaining -= L;
      idx = (idx+1)%g_path.n;
      target = g_path.p[idx];
    }
  }
  // Steering towards target
  float desired = atan2f(target.y - c->y, target.x - c->x);
  float diff = wrap_angle(desired - c->ang);
  c->steer = clampf(diff / (float)M_PI * 3.0f, -1.0f, 1.0f);

  // Throttle control based on curvature ahead and slip
  Vec2 A = g_path.p[i];
  Vec2 B = g_path.p[(i+1)%g_path.n];
  Vec2 Cn = g_path.p[(i+2)%g_path.n];
  float ang1 = atan2f(B.y - A.y, B.x - A.x);
  float ang2 = atan2f(Cn.y - B.y, Cn.x - B.x);
  float turn = fabsf(wrap_angle(ang2 - ang1));
  float speed = sqrtf(c->vx*c->vx + c->vy*c->vy);
  float tgt = 9.0f - 5.5f * clampf(turn/1.5f, 0.0f, 1.0f); // slow for sharp turns
  // also slow if off road
  char T = tile_at(c->x, c->y);
  if (T!='.' && T!='|' && T!='N') tgt *= 0.75f;
  float diffv = tgt - speed;
  c->throttle = clampf(diffv*0.6f, -0.6f, 1.0f);

  // Rare nitro usage on long straights
  if (c->nitro > 0 && turn < 0.18f && speed < 8.5f && (rand()%400)==0) {
    c->nitro_time = 1.2f;
    c->nitro--;
  }
}

// Player control from input
static void player_control(Car* c, const Input* in) {
  float left  = (in->left || in->a) ? 1.0f : 0.0f;
  float right = (in->right|| in->d) ? 1.0f : 0.0f;
  float up    = (in->up   || in->w) ? 1.0f : 0.0f;
  float down  = (in->down || in->s) ? 1.0f : 0.0f;

  c->steer = (right - left);
  c->steer = clampf(c->steer, -1.0f, 1.0f);

  c->throttle = (up - 0.6f*down);
  c->throttle = clampf(c->throttle, -1.0f, 1.0f);
}

// Rendering buffer
static char scr[SCREEN_H][SCREEN_W];
static Color scn[SCREEN_H][SCREEN_W];

static void clear_screen_buf() {
  for (int y=0;y<SCREEN_H;y++){
    for (int x=0;x<SCREEN_W;x++){
      scr[y][x] = ' ';
      scn[y][x] = COL_DEFAULT;
    }
  }
}

static void draw_hud(int lap, int pos, int total, float speed, int nitro, int countdown, int finished, float elapsed) {
  char bar[SCREEN_W+1]; memset(bar, ' ', SCREEN_W); bar[SCREEN_W]=0;
  snprintf(bar, SCREEN_W, " Super ASCII Offroad   Lap %d/%d   Pos %d/%d   Speed %.1f   Nitro %d   Time %.1fs",
           lap, TARGET_LAPS, pos, total, speed, nitro, elapsed);
  for (int x=0;x<SCREEN_W && bar[x];x++){
    scr[0][x] = bar[x];
    scn[0][x] = COL_HUD;
  }

  char bar2[SCREEN_W+1]; memset(bar2, ' ', SCREEN_W); bar2[SCREEN_W]=0;
  if (countdown > 0) {
    snprintf(bar2, SCREEN_W, " Get Ready... %d ", countdown);
  } else if (finished) {
    snprintf(bar2, SCREEN_W, " Finish! Press R to restart, Q to quit ");
  } else {
    snprintf(bar2, SCREEN_W, " Controls: Arrows/WASD steer/throttle, Space = Nitro, R = Reset, P = Pause, Q = Quit ");
  }
  for (int x=0;x<SCREEN_W && bar2[x];x++){
    scr[1][x] = bar2[x];
    scn[1][x] = COL_HUD;
  }
}

static void draw_track_to_buf() {
  for (int y=0;y<TRACK_H;y++){
    for (int x=0;x<TRACK_W;x++){
      scr[HUD_ROWS+y][x] = g_tile[y][x];
      scn[HUD_ROWS+y][x] = g_color[y][x];
    }
  }
  // Active items
  for (int i=0;i<g_items_n;i++){
    if (!g_items[i].active) continue;
    int x = (int)g_items[i].x, y = (int)g_items[i].y;
    if (x>=0 && x<TRACK_W && y>=0 && y<TRACK_H) {
      scr[HUD_ROWS+y][x] = 'N';
      scn[HUD_ROWS+y][x] = COL_NITRO;
    }
  }
}

static void put_car(const Car* c) {
  int x = (int)floorf(c->x);
  int y = (int)floorf(c->y);
  if (x<0||y<0||x>=TRACK_W||y>=TRACK_H) return;

  // Use heading char
  char ch = '>';
  float a = wrap_angle(c->ang);
  float deg = a * 180.0f / (float)M_PI;
  if (deg < 0) deg += 360.0f;
  if (deg >= 315 || deg < 45) ch = '>';
  else if (deg < 135) ch = 'v';
  else if (deg < 225) ch = '<';
  else ch = '^';

  scr[HUD_ROWS+y][x] = ch;
  scn[HUD_ROWS+y][x] = c->color;
}

static void flush_screen() {
  console_goto(1,1);
  Color last = COL_DEFAULT;
  for (int y=0;y<SCREEN_H;y++){
    for (int x=0;x<SCREEN_W;x++){
      Color c = scn[y][x];
      if (c != last) { console_color(c); last = c; }
      char ch = scr[y][x];
      // ensure printable
      if (ch == 0) ch = ' ';
      putchar(ch);
    }
    // newline
    if (last != COL_DEFAULT) { printf(CLR_RESET); last = COL_DEFAULT; }
    if (y < SCREEN_H-1) putchar('\n');
  }
  if (last != COL_DEFAULT) printf(CLR_RESET);
  fflush(stdout);
}

static int cmp_progress_desc(const void* a, const void* b) {
  const Car* ca = *(const Car* const*)a;
  const Car* cb = *(const Car* const*)b;
  if (ca->progress > cb->progress) return -1;
  if (ca->progress < cb->progress) return 1;
  return 0;
}

int main(void) {
  srand((unsigned int)time(NULL));
  console_init();

  init_path(&g_path);
  build_track();
  spawn_cars();

  int race_finished = 0;
  int countdown = 3;
  double start_t = now_sec();
  double last_t = start_t;
  double acc = 0.0;
  double countdown_t = start_t;

  // Simple countdown
  while (countdown > 0) {
    double t = now_sec();
    if (t - countdown_t >= 1.0) {
      countdown--; countdown_t = t;
    }
    clear_screen_buf();
    draw_track_to_buf();
    // draw cars idle on grid
    for (int i=0;i<MAX_CARS;i++) put_car(&g_cars[i]);
    draw_hud(0, 1, MAX_CARS, 0.0f, g_cars[0].nitro, countdown, 0, 0.0f);
    flush_screen();
    usleep(1000000/FPS);
    Input in={0}; poll_input(&in);
    if (in.quit) { console_shutdown(); return 0; }
  }

  double race_t0 = now_sec();
  while (1) {
    double t = now_sec();
    double dt = t - last_t;
    if (dt > 0.1) dt = 0.1;
    acc += dt;
    last_t = t;

    // Fixed-step update at FPS
    int steps = 0;
    while (acc >= DT) {
      acc -= DT;
      steps++;
      // Poll input once per fixed dt (kept simple)
      Input in={0};
      poll_input(&in);
      if (in.quit) { console_shutdown(); return 0; }
      if (in.reset) {
        build_track();
        spawn_cars();
        race_finished = 0;
        race_t0 = now_sec();
        break; // redraw immediately
      }
      if (in.pause) {
        // simple pause
        clear_screen_buf();
        draw_track_to_buf();
        for (int i=0;i<MAX_CARS;i++) put_car(&g_cars[i]);
        draw_hud(g_cars[0].laps, 1, MAX_CARS, 0.0f, g_cars[0].nitro, 0, race_finished, (float)(now_sec()-race_t0));
        // overlay paused text
        const char* msg = "Paused - press P to resume";
        int x0 = (SCREEN_W - (int)strlen(msg))/2;
        int y0 = HUD_ROWS + TRACK_H/2;
        for (int i=0; msg[i]; i++){
          scr[y0][x0+i] = msg[i]; scn[y0][x0+i] = COL_HUD;
        }
        flush_screen();
        // wait until P pressed again or Q
        while (1) {
          Input in2={0}; poll_input(&in2);
          if (in2.quit) { console_shutdown(); return 0; }
          if (in2.pause) break;
          usleep(1000*1000/FPS);
        }
      }

      // Human control
      if (!race_finished) {
        if (in.nitro && g_cars[0].nitro > 0 && g_cars[0].nitro_time <= 0) {
          g_cars[0].nitro_time = 1.5f;
          g_cars[0].nitro--;
        }
        player_control(&g_cars[0], &in);
      } else {
        g_cars[0].steer = 0;
        g_cars[0].throttle = 0;
      }

      // AI control
      for (int i=1;i<MAX_CARS;i++){
        ai_control(&g_cars[i], (float)DT, i);
      }

      // Physics step
      for (int i=0;i<MAX_CARS;i++){
        physics_car(&g_cars[i], (float)DT);
      }

      // Simple car-car collision (elastic-ish) to prevent overlap
      for (int i=0;i<MAX_CARS;i++){
        for (int j=i+1;j<MAX_CARS;j++){
          float dx = g_cars[i].x - g_cars[j].x;
          float dy = g_cars[i].y - g_cars[j].y;
          float d2 = dx*dx + dy*dy;
          float minr = 0.7f;
          if (d2 < minr*minr) {
            float d = sqrtf(d2)+1e-6f;
            float nx = dx/d, ny = dy/d;
            float overlap = minr - d;
            // push apart
            g_cars[i].x += nx * (overlap*0.5f);
            g_cars[i].y += ny * (overlap*0.5f);
            g_cars[j].x -= nx * (overlap*0.5f);
            g_cars[j].y -= ny * (overlap*0.5f);
            // bounce velocities
            float rvx = g_cars[i].vx - g_cars[j].vx;
            float rvy = g_cars[i].vy - g_cars[j].vy;
            float rel = rvx*nx + rvy*ny;
            float imp = -0.6f * rel;
            g_cars[i].vx += imp * nx * 0.5f;
            g_cars[i].vy += imp * ny * 0.5f;
            g_cars[j].vx -= imp * nx * 0.5f;
            g_cars[j].vy -= imp * ny * 0.5f;
          }
        }
      }

      // Items respawn timers
      for (int i=0;i<g_items_n;i++){
        if (!g_items[i].active) {
          g_items[i].respawn_timer -= (float)DT;
          if (g_items[i].respawn_timer <= 0) {
            // Reactivate only if still on road
            int x = (int)g_items[i].x, y = (int)g_items[i].y;
            if (x>=0 && x<TRACK_W && y>=0 && y<TRACK_H && g_tile[y][x]=='.') {
              g_items[i].active = 1;
            } else {
              // move slightly along path
              g_items[i].respawn_timer = 2.0f;
            }
          }
        }
      }

      // Finish condition
      if (!race_finished && g_cars[0].laps >= TARGET_LAPS) {
        race_finished = 1;
      }
    }

    // Compose leaderboard for HUD
    Car* order[MAX_CARS];
    for (int i=0;i<MAX_CARS;i++) order[i] = &g_cars[i];
    qsort(order, MAX_CARS, sizeof(order[0]), cmp_progress_desc);
    int player_pos = 1;
    for (int i=0;i<MAX_CARS;i++){
      if (order[i]->id == 0) { player_pos = i+1; break; }
    }

    // Render
    clear_screen_buf();
    draw_track_to_buf();
    for (int i=0;i<MAX_CARS;i++) put_car(&g_cars[i]);
    float pspeed = sqrtf(g_cars[0].vx*g_cars[0].vx + g_cars[0].vy*g_cars[0].vy);
    int cd = 0;
    draw_hud(g_cars[0].laps, player_pos, MAX_CARS, pspeed, g_cars[0].nitro, cd, race_finished, (float)(now_sec()-race_t0));
    flush_screen();

    // Frame pacing
    double frame_time = now_sec() - t;
    double target = 1.0 / FPS;
    if (frame_time < target) {
      int us = (int)((target - frame_time) * 1000000.0);
      if (us > 0) usleep(us);
    }
  }

  console_shutdown();
  return 0;
}
