// ASCII Super Sprint - Single file C, no external deps
// Cross-platform terminal (POSIX + Windows). C99.
// Controls: W/A/S/D to drive, Q quit, R restart after finish.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <termios.h>
  #include <fcntl.h>
  #include <sys/time.h>
#endif

// -------------------- Config --------------------
#define SCR_W 80
#define SCR_H 30

#define TRACK_RADIUS 3.0f   // half-width of road in cells
#define TARGET_FPS 60.0
#define MAX_CARS 4          // 1 player + 3 AI
#define AI_COUNT 3
#define LAPS_TO_WIN 3

// Physics
#define ACCEL 18.0f
#define BRAKE 22.0f
#define MAX_SPEED 14.0f
#define FRICTION 3.2f
#define TURN_RATE 2.9f      // rad/s at speed ~ MAX_SPEED
#define BOUNCE 0.2f
#define CAR_RADIUS 0.35f

// AI
#define AI_MAX_SPEED 12.0f
#define AI_LOOKAHEAD 7.0f
#define AI_STEER_GAIN 2.2f
#define AI_THROTTLE_GAIN 6.0f

// -------------------- Math --------------------
typedef struct { float x, y; } Vec2;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float dotv(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
static inline Vec2 addv(Vec2 a, Vec2 b) { Vec2 r = {a.x+b.x, a.y+b.y}; return r; }
static inline Vec2 subv(Vec2 a, Vec2 b) { Vec2 r = {a.x-b.x, a.y-b.y}; return r; }
static inline Vec2 mulv(Vec2 a, float s) { Vec2 r = {a.x*s, a.y*s}; return r; }
static inline float lenv(Vec2 a) { return sqrtf(a.x*a.x + a.y*a.y); }
static inline Vec2 normv(Vec2 a) { float l = lenv(a); if (l < 1e-6f) { Vec2 z={0,0}; return z; } return mulv(a, 1.0f/l); }
static inline float wrapf(float x, float m) { float r = fmodf(x, m); if (r < 0) r += m; return r; }
static inline float wrap_angle(float a) {
    while (a > (float)M_PI) a -= 2.0f*(float)M_PI;
    while (a < -(float)M_PI) a += 2.0f*(float)M_PI;
    return a;
}

// -------------------- Platform --------------------
#ifdef _WIN32
static LARGE_INTEGER qpfreq;
static double now_sec() {
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)qpfreq.QuadPart;
}
static void sleep_ms(int ms) { Sleep((DWORD)ms); }

static void platform_init() {
    QueryPerformanceFrequency(&qpfreq);
    // Enable ANSI VT
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hout, &mode)) {
        mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hout, mode);
    }
    // Hide cursor
    CONSOLE_CURSOR_INFO ci;
    ci.dwSize = 1; ci.bVisible = FALSE;
    SetConsoleCursorInfo(hout, &ci);
    // Clear screen
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);
}
static void platform_shutdown() {
    // Show cursor
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    ci.dwSize = 1; ci.bVisible = TRUE;
    SetConsoleCursorInfo(hout, &ci);
    printf("\x1b[?25h\x1b[0m\x1b[2J\x1b[H");
    fflush(stdout);
}
typedef struct {
    bool w,a,s,d,q,r;
} InputState;
static InputState input_poll() {
    InputState in = {0};
    short W = GetAsyncKeyState('W');
    short A = GetAsyncKeyState('A');
    short S = GetAsyncKeyState('S');
    short D = GetAsyncKeyState('D');
    short Q = GetAsyncKeyState('Q');
    short R = GetAsyncKeyState('R');
    in.w = (W & 0x8000) != 0;
    in.a = (A & 0x8000) != 0;
    in.s = (S & 0x8000) != 0;
    in.d = (D & 0x8000) != 0;
    in.q = (Q & 0x8000) != 0;
    in.r = (R & 0x8000) != 0;
    return in;
}
#else
static struct termios orig_term;
static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
static void sleep_ms(int ms) {
    struct timespec ts = { ms/1000, (ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}
static void platform_init() {
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\x1b[2J\x1b[H\x1b[?25l");
    fflush(stdout);
    // set stdin nonblocking (optional)
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}
static void platform_shutdown() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
    printf("\x1b[?25h\x1b[0m\x1b[2J\x1b[H");
    fflush(stdout);
}
typedef struct {
    bool w,a,s,d,q,r;
} InputState;
static InputState input_poll() {
    InputState in = {0};
    unsigned char c;
    // drain available bytes
    while (read(STDIN_FILENO, &c, 1) > 0) {
        if (c=='w' || c=='W') in.w = true;
        else if (c=='a' || c=='A') in.a = true;
        else if (c=='s' || c=='S') in.s = true;
        else if (c=='d' || c=='D') in.d = true;
        else if (c=='q' || c=='Q') in.q = true;
        else if (c=='r' || c=='R') in.r = true;
        // Ignore others (arrow keys are escape sequences; using WASD avoids parsing)
    }
    return in;
}
#endif

// -------------------- Track / Path --------------------
typedef struct {
    int w, h;
    char cells[SCR_H][SCR_W+1];  // +1 for '\0'
    unsigned char solid[SCR_H][SCR_W]; // 1=wall, 0=drivable
    unsigned char startmask[SCR_H][SCR_W]; // start line marker
} Grid;

typedef struct {
    Vec2* pts;
    int n;
    float* seglen;
    float* cumlen; // size n
    float length;
} Path;

// Handcrafted path (loop) inside 80x30
static Vec2 PATH_PTS[] = {
    {62,16}, {72,12}, {70, 7}, {52, 5}, {36, 4}, {22, 5},
    {12, 8}, { 8,12}, { 8,18}, {12,23}, {24,26}, {40,28},
    {54,27}, {66,24}, {74,20}, {76,16}, {74,12}, {68,10},
    {62,12}
};
#define PATH_N (int)(sizeof(PATH_PTS)/sizeof(PATH_PTS[0]))

static void path_build(Path* p) {
    p->n = PATH_N;
    p->pts = PATH_PTS;
    p->seglen = (float*)malloc(sizeof(float)*p->n);
    p->cumlen = (float*)malloc(sizeof(float)*p->n);
    p->length = 0.0f;
    for (int i=0;i<p->n;i++) {
        Vec2 a = p->pts[i];
        Vec2 b = p->pts[(i+1)%p->n];
        float l = lenv(subv(b,a));
        p->seglen[i] = l;
        p->cumlen[i] = p->length;
        p->length += l;
    }
}

static void path_free(Path* p) {
    free(p->seglen);
    free(p->cumlen);
    p->seglen = p->cumlen = NULL;
}

static Vec2 path_point_at(const Path* p, float s) {
    float L = p->length;
    s = wrapf(s, L);
    // find segment
    int i = 0;
    // linear scan is fine (n is small)
    while (i < p->n-1 && !(p->cumlen[i] <= s && s <= p->cumlen[i] + p->seglen[i])) i++;
    float segS = s - p->cumlen[i];
    float t = (p->seglen[i] > 1e-6f) ? segS / p->seglen[i] : 0.0f;
    Vec2 a = p->pts[i];
    Vec2 b = p->pts[(i+1)%p->n];
    Vec2 r = { lerpf(a.x,b.x,t), lerpf(a.y,b.y,t) };
    return r;
}
static Vec2 path_tangent_at(const Path* p, float s) {
    s = wrapf(s, p->length);
    int i = 0;
    while (i < p->n-1 && !(p->cumlen[i] <= s && s <= p->cumlen[i] + p->seglen[i])) i++;
    Vec2 a = p->pts[i];
    Vec2 b = p->pts[(i+1)%p->n];
    return normv(subv(b,a));
}
static float path_s_of_point(const Path* p, Vec2 pos) {
    float bestD2 = 1e9f;
    float bestS = 0.0f;
    for (int i=0;i<p->n;i++) {
        Vec2 a = p->pts[i];
        Vec2 b = p->pts[(i+1)%p->n];
        Vec2 ab = subv(b,a);
        float ab2 = dotv(ab,ab);
        float t = 0.0f;
        if (ab2 > 1e-6f) t = clampf( dotv(subv(pos,a), ab) / ab2, 0.0f, 1.0f );
        Vec2 c = addv(a, mulv(ab, t));
        float dx = pos.x - c.x;
        float dy = pos.y - c.y;
        float d2 = dx*dx + dy*dy;
        if (d2 < bestD2) {
            bestD2 = d2;
            bestS = p->cumlen[i] + t * p->seglen[i];
        }
    }
    return wrapf(bestS, p->length);
}

// Build grid: fill walls '#', carve road along polyline with thickness TRACK_RADIUS
static void build_grid(const Path* path, Grid* g) {
    g->w = SCR_W; g->h = SCR_H;
    memset(g->solid, 1, sizeof(g->solid));
    memset(g->startmask, 0, sizeof(g->startmask));
    for (int y=0;y<SCR_H;y++) {
        for (int x=0;x<SCR_W;x++) {
            g->cells[y][x] = '#';
        }
        g->cells[y][SCR_W] = '\0';
    }
    // Carve road
    int stepsPerUnit = 5;
    for (int i=0;i<path->n;i++) {
        Vec2 a = path->pts[i];
        Vec2 b = path->pts[(i+1)%path->n];
        Vec2 ab = subv(b,a);
        float L = fmaxf(1.0f, lenv(ab));
        int steps = (int)(L * stepsPerUnit);
        for (int s=0;s<=steps;s++) {
            float t = (float)s / (float)steps;
            float fx = lerpf(a.x,b.x,t);
            float fy = lerpf(a.y,b.y,t);
            int cx = (int)floorf(fx + 0.5f);
            int cy = (int)floorf(fy + 0.5f);
            for (int oy = -(int)(TRACK_RADIUS+1); oy <= (int)(TRACK_RADIUS+1); oy++) {
                for (int ox = -(int)(TRACK_RADIUS+1); ox <= (int)(TRACK_RADIUS+1); ox++) {
                    int x = cx + ox;
                    int y = cy + oy;
                    if (x<=0 || x>=SCR_W-1 || y<=0 || y>=SCR_H-1) continue;
                    float dd = (float)(ox*ox + oy*oy);
                    if (dd <= (TRACK_RADIUS*TRACK_RADIUS)) {
                        g->solid[y][x] = 0;
                        g->cells[y][x] = ' ';
                    }
                }
            }
        }
    }
    // Start line at path start, perpendicular to first segment
    Vec2 p0 = path->pts[0];
    Vec2 p1 = path->pts[1];
    Vec2 dir = normv(subv(p1,p0));
    Vec2 n = (Vec2){ -dir.y, dir.x };
    for (int t=- (int)TRACK_RADIUS; t <= (int)TRACK_RADIUS; t++) {
        Vec2 spt = addv(p0, mulv(n, (float)t));
        int x = (int)floorf(spt.x + 0.5f);
        int y = (int)floorf(spt.y + 0.5f);
        if (x>0 && x<SCR_W-1 && y>0 && y<SCR_H-1) {
            g->cells[y][x] = '|';
            g->solid[y][x] = 0;
            g->startmask[y][x] = 1;
        }
    }
    // Make sure border stays walls '#'
    for (int x=0;x<SCR_W;x++) { g->cells[0][x]='#'; g->cells[SCR_H-1][x]='#'; g->solid[0][x]=1; g->solid[SCR_H-1][x]=1; }
    for (int y=0;y<SCR_H;y++) { g->cells[y][0]='#'; g->cells[y][SCR_W-1]='#'; g->solid[y][0]=1; g->solid[y][SCR_W-1]=1; }
}

// -------------------- Cars / Race --------------------
typedef struct {
    Vec2 pos;
    float ang;
    float vel;
    float sPrev;
    float lapAccum;    // forward progress accumulator
    int laps;
    int finished;
    float finishTime;
    float ai_s;        // AI's target along path
    char glyph;
    char name[16];
    bool isAI;
} Car;

typedef struct {
    Path path;
    Grid grid;
    Car cars[MAX_CARS];
    int carCount;
    double startTime;
    double raceTime;
    double endTime;
    int state; // 0=COUNTDOWN,1=RACING,2=FINISHED
    int playerIndex;
} Game;

// Collision check against walls using small radius
static bool collide_wall(const Grid* g, Vec2 p) {
    int ix = (int)floorf(p.x + 0.5f);
    int iy = (int)floorf(p.y + 0.5f);
    if (ix < 1 || ix >= g->w-1 || iy < 1 || iy >= g->h-1) return true;
    // Check a small disc area
    for (int oy=-1; oy<=1; oy++) {
        for (int ox=-1; ox<=1; ox++) {
            int x = ix + ox, y = iy + oy;
            if (x<0 || y<0 || x>=g->w || y>=g->h) return true;
            if (g->solid[y][x]) return true;
        }
    }
    return false;
}

// Simple car separation to avoid overlap
static void cars_separate(Car* a, Car* b) {
    Vec2 d = subv(b->pos, a->pos);
    float dist = lenv(d);
    float minDist = 2.0f*CAR_RADIUS + 0.2f;
    if (dist > 1e-4f && dist < minDist) {
        Vec2 push = mulv(d, (minDist - dist) / dist * 0.5f);
        a->pos = subv(a->pos, push);
        b->pos = addv(b->pos, push);
        // Damp velocities a bit
        a->vel *= 0.9f;
        b->vel *= 0.9f;
    }
}

static void cars_spawn(Game* g) {
    // Spawn cars along start line, offset sideways
    Vec2 p0 = g->path.pts[0];
    Vec2 p1 = g->path.pts[1];
    Vec2 dir = normv(subv(p1,p0));
    Vec2 n = (Vec2){ -dir.y, dir.x };

    int N = g->carCount;
    float offsets[] = { -1.5f, -0.5f, 0.5f, 1.5f };
    for (int i=0;i<N;i++) {
        Car* c = &g->cars[i];
        float off = (i < 4) ? offsets[i] : (float)i - (N-1)*0.5f;
        c->pos = addv(p0, mulv(n, off));
        c->ang = atan2f(dir.y, dir.x);
        c->vel = 0.0f;
        c->sPrev = path_s_of_point(&g->path, c->pos);
        c->lapAccum = 0.0f;
        c->laps = 0;
        c->finished = 0;
        c->finishTime = 0.0f;
        c->ai_s = c->sPrev;
    }
    // assign glyphs/names
    g->cars[0].glyph = '1'; snprintf(g->cars[0].name, sizeof(g->cars[0].name), "YOU");
    for (int i=1;i<N;i++) {
        g->cars[i].glyph = '1' + i;
        snprintf(g->cars[i].name, sizeof(g->cars[i].name), "AI%d", i);
    }
}

static void game_init(Game* g) {
    memset(g, 0, sizeof(*g));
    path_build(&g->path);
    build_grid(&g->path, &g->grid);
    g->carCount = 1 + AI_COUNT;
    g->playerIndex = 0;
    for (int i=0;i<g->carCount;i++) {
        g->cars[i].isAI = (i != g->playerIndex);
    }
    cars_spawn(g);
    g->startTime = now_sec();
    g->raceTime = 0.0;
    g->endTime = 0.0;
    g->state = 0; // countdown
}

static void game_reset(Game* g) {
    // Keep same path/grid
    for (int i=0;i<g->carCount;i++) {
        memset(&g->cars[i], 0, sizeof(Car));
        g->cars[i].isAI = (i != g->playerIndex);
    }
    cars_spawn(g);
    g->startTime = now_sec();
    g->raceTime = 0.0;
    g->endTime = 0.0;
    g->state = 0;
}

static void car_update_player(Game* g, Car* c, const InputState* in, float dt, bool controls_enabled) {
    float turn = 0.0f;
    if (controls_enabled) {
        if (in->a) turn -= 1.0f;
        if (in->d) turn += 1.0f;
        if (in->w) c->vel += ACCEL * dt;
        if (in->s) c->vel -= BRAKE * dt;
    }
    // friction
    float f = FRICTION * dt;
    if (c->vel > 0) { c->vel = fmaxf(0.0f, c->vel - f); }
    else if (c->vel < 0) { c->vel = fminf(0.0f, c->vel + f); }
    c->vel = clampf(c->vel, -MAX_SPEED*0.5f, MAX_SPEED);

    // Turn more at higher speeds
    float speedFactor = clampf(fabsf(c->vel)/MAX_SPEED, 0.2f, 1.0f);
    c->ang += turn * TURN_RATE * speedFactor * dt;

    // integrate
    Vec2 dir = { cosf(c->ang), sinf(c->ang) };
    Vec2 prev = c->pos;
    c->pos = addv(c->pos, mulv(dir, c->vel * dt));
    // collisions
    if (collide_wall(&g->grid, c->pos)) {
        c->pos = prev;
        c->vel = -c->vel * BOUNCE;
    }
}

static void car_update_ai(Game* g, Car* c, float dt, float ai_speed_bias) {
    // advance target along the path
    c->ai_s = wrapf(c->ai_s + fmaxf(0.0f, c->vel)*dt + 4.0f*dt, g->path.length);
    float look = AI_LOOKAHEAD + ai_speed_bias * 2.0f;
    Vec2 target = path_point_at(&g->path, c->ai_s + look);
    Vec2 toT = subv(target, c->pos);
    float desired = atan2f(toT.y, toT.x);
    float diff = wrap_angle(desired - c->ang);

    // steer
    c->ang += clampf(diff, -1.0f, 1.0f) * AI_STEER_GAIN * dt;

    // desired speed: slower when turning sharply
    float turnFactor = 1.0f - clampf(fabsf(diff) / 1.4f, 0.0f, 0.8f);
    float vDes = (AI_MAX_SPEED + ai_speed_bias) * (0.4f + 0.6f * turnFactor);

    // throttle towards vDes
    float dv = vDes - c->vel;
    c->vel += AI_THROTTLE_GAIN * dv * dt;

    // friction and clamp
    float f = FRICTION * dt;
    if (c->vel > 0) c->vel = fmaxf(0.0f, c->vel - f);
    else c->vel = fminf(0.0f, c->vel + f);
    c->vel = clampf(c->vel, -MAX_SPEED*0.5f, MAX_SPEED*0.95f);

    // move
    Vec2 dir = { cosf(c->ang), sinf(c->ang) };
    Vec2 prev = c->pos;
    c->pos = addv(c->pos, mulv(dir, c->vel * dt));
    if (collide_wall(&g->grid, c->pos)) {
        c->pos = prev;
        c->vel = -c->vel * 0.1f;
        c->ang += 0.5f * ((rand()%2)?1.0f:-1.0f) * dt; // jiggle
    }
}

static void update_laps_and_progress(Game* g, Car* c, double raceTime) {
    float sCurr = path_s_of_point(&g->path, c->pos);
    float L = g->path.length;
    float d = sCurr - c->sPrev;
    // wrap-aware delta
    if (d >  L*0.5f) d -= L;
    if (d < -L*0.5f) d += L;
    if (d > 0) c->lapAccum += d;
    c->sPrev = sCurr;

    // Check start line crossing: near s ~ 0
    float startZone = 2.0f; // cells' worth of arc-length
    bool crossedStart = ( (sCurr < startZone) && (c->sPrev > L - startZone) ); // note: sPrev updated before? keep old copy
    // Actually we used sPrev then replaced with sCurr; fix by computing separately:
    // We'll recompute properly:

    // recompute using previous value kept earlier: store prev before update
}

// Render and UI
static void render(const Game* g) {
    // Copy base grid to frame
    char frame[SCR_H][SCR_W+1];
    for (int y=0;y<SCR_H;y++) {
        memcpy(frame[y], g->grid.cells[y], SCR_W+1);
    }
    // Overlay cars
    for (int i=0;i<g->carCount;i++) {
        const Car* c = &g->cars[i];
        int x = (int)floorf(c->pos.x + 0.5f);
        int y = (int)floorf(c->pos.y + 0.5f);
        if (x>=1 && x<SCR_W-1 && y>=1 && y<SCR_H-1) {
            frame[y][x] = c->glyph;
        }
    }

    // Move cursor home
    printf("\x1b[H");
    // Header line
    const Car* player = &g->cars[g->playerIndex];
    double t = g->raceTime;
    int mins = (int)(t/60.0); double secs = t - mins*60.0;
    // Compute position
    int pos = 1;
    float L = g->path.length;
    float pProg = player->laps * L + player->sPrev;
    for (int i=0;i<g->carCount;i++) {
        if (i==g->playerIndex) continue;
        const Car* c = &g->cars[i];
        float prog = c->laps * L + c->sPrev;
        if (prog > pProg) pos++;
    }
    printf("ASCII Super Sprint   Laps: %d/%d   Time: %02d:%05.2f   Pos: %d/%d   Speed: %.1f    \n",
           player->laps, LAPS_TO_WIN, mins, secs, pos, g->carCount, player->vel);

    // Countdown / messages
    if (g->state == 0) {
        double elapsed = now_sec() - g->startTime;
        int remain = (int)(3.0 - elapsed) + 1;
        if (remain < 1) remain = 1;
        printf("Start in: %d    (WASD to drive, Q to quit)\n", remain);
    } else if (g->state == 1) {
        printf("GO! (WASD)  Q to quit\n");
    } else {
        const Car* winner = NULL;
        int winIndex = -1;
        for (int i=0;i<g->carCount;i++) if (g->cars[i].finished) { winner = &g->cars[i]; winIndex = i; break; }
        if (winner) {
            const char* res = (winIndex == g->playerIndex) ? "YOU WIN!" : "YOU LOSE!";
            printf("%s   Press R to restart, Q to quit.  Winner: %s  Time: %.2fs\n", res, winner->name, winner->finishTime);
        } else {
            printf("Race over. Press R to restart, Q to quit.\n");
        }
    }

    // Draw track
    for (int y=0;y<SCR_H;y++) {
        fwrite(frame[y], 1, SCR_W, stdout);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

static void update_game(Game* g, const InputState* in, float dt) {
    double nowt = now_sec();
    if (g->state == 0) { // countdown
        double elapsed = nowt - g->startTime;
        if (elapsed >= 3.0) {
            g->state = 1;
            g->raceTime = 0.0;
        } else {
            g->raceTime = 0.0;
        }
    } else if (g->state == 1) { // racing
        g->raceTime += dt;
    }

    bool controls_enabled = (g->state == 1);
    // Update cars
    for (int i=0;i<g->carCount;i++) {
        Car* c = &g->cars[i];
        if (c->finished) continue;

        float prevS = c->sPrev;

        if (i == g->playerIndex) {
            car_update_player(g, c, in, dt, controls_enabled);
        } else {
            float bias = 0.0f;
            // Give slight variety to AIs
            if (i == 1) bias = +0.5f;
            if (i == 2) bias = -0.5f;
            if (i == 3) bias = +1.0f;
            car_update_ai(g, c, dt, bias);
        }

        // separation (naive)
        for (int j=i+1;j<g->carCount;j++) {
            if (!g->cars[j].finished) cars_separate(c, &g->cars[j]);
        }

        // Laps / progress
        float sCurr = path_s_of_point(&g->path, c->pos);
        float L = g->path.length;
        float d = sCurr - prevS;
        if (d >  L*0.5f) d -= L;
        if (d < -L*0.5f) d += L;
        if (d > 0) c->lapAccum += d;
        c->sPrev = sCurr;

        // start line crossing from high s to near zero
        float startZone = 2.0f;
        bool crossedStart = (prevS > L - startZone) && (sCurr < startZone);
        if (controls_enabled && crossedStart) {
            if (c->lapAccum >= 0.65f * L) {
                c->laps += 1;
                c->lapAccum = 0.0f;
                if (c->laps >= LAPS_TO_WIN && !c->finished) {
                    c->finished = 1;
                    c->finishTime = (float)g->raceTime;
                }
            } else {
                // Anti-cheat: ignore tiny loops
                c->lapAccum = 0.0f;
            }
        }
    }

    // Determine race end
    if (g->state == 1) {
        int finisher = -1;
        float bestTime = 1e9f;
        for (int i=0;i<g->carCount;i++) {
            if (g->cars[i].finished && g->cars[i].finishTime < bestTime) {
                bestTime = g->cars[i].finishTime;
                finisher = i;
            }
        }
        if (finisher >= 0) {
            g->state = 2;
            g->endTime = nowt;
        }
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
    platform_init();

    Game game;
    game_init(&game);

    double last = now_sec();
    bool running = true;

    while (running) {
        double nowt = now_sec();
        float dt = (float)(nowt - last);
        last = nowt;
        if (dt > 0.05f) dt = 0.05f; // clamp to avoid huge jumps

        InputState in = input_poll();
        if (in.q) { running = false; break; }
        if (game.state == 2 && in.r) {
            game_reset(&game);
            // reset timing origin to avoid large dt
            last = now_sec();
            continue;
        }

        update_game(&game, &in, dt);
        render(&game);

        // Frame limiting
        double frameTime = now_sec() - nowt;
        double target = 1.0 / TARGET_FPS;
        if (frameTime < target) {
            sleep_ms((int)((target - frameTime) * 1000.0));
        }
    }

    path_free(&game.path);
    platform_shutdown();
    return 0;
}
