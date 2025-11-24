/*
 * SUPER SPRINT STYLE RACER
 * Single File C Program - Win32 API - No External Dependencies
 *
 * Controls: Arrow Keys to Drive.
 * Goal: Drive laps around the track.
 */

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

// --- Constants & Configuration ---
#define SCREEN_W 800
#define SCREEN_H 600
#define PI 3.14159265f
#define FPS 60

// Physics Constants (Super Sprint Feel)
#define ACCELERATION 0.15f
#define MAX_SPEED 9.0f
#define TURN_SPEED 0.06f
#define DRAG 0.97f          // Air resistance (slows down when not gas)
#define DRIFT_FACTOR 0.94f  // Lateral friction (lower = more drift/ice)
#define WALL_BOUNCE 0.4f    // How much you bounce off walls

// --- Math Utils ---
typedef struct { float x, y; } Vec2;

Vec2 v_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec2 v_sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
Vec2 v_mul(Vec2 a, float s) { return (Vec2){a.x * s, a.y * s}; }
float v_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
float v_len(Vec2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
Vec2 v_norm(Vec2 a) { float l = v_len(a); return (l == 0) ? (Vec2){0,0} : v_mul(a, 1.0f/l); }

// --- Game State ---
typedef struct {
    Vec2 pos;
    Vec2 vel;
    float angle; // Radians
    float radius;
    int color;
} Car;

// Track Data: Defined as a series of points making line segments
#define MAX_WALLS 100
typedef struct {
    Vec2 points[MAX_WALLS];
    int count;
} WallChain;

WallChain outer_wall;
WallChain inner_wall;
Car p1;

// Input State
bool key_up = false, key_down = false, key_left = false, key_right = false;
bool game_running = true;

// --- Track Setup ---
void InitTrack() {
    // Hardcoded track coordinates to resemble a twisted circuit
    // Outer Boundary
    outer_wall.count = 0;
    Vec2 out[] = {
        {50, 50}, {400, 50}, {750, 50}, {750, 300}, {750, 550}, 
        {400, 550}, {50, 550}, {50, 300}
    };
    // Interpolate slightly for smoother look or just use lines
    // For simplicity, we define corners.
    outer_wall.points[0] = (Vec2){50, 100};
    outer_wall.points[1] = (Vec2){50, 50};
    outer_wall.points[2] = (Vec2){750, 50};
    outer_wall.points[3] = (Vec2){750, 550};
    outer_wall.points[4] = (Vec2){50, 550};
    outer_wall.points[5] = (Vec2){50, 450};
    outer_wall.points[6] = (Vec2){300, 450};
    outer_wall.points[7] = (Vec2){400, 350};
    outer_wall.points[8] = (Vec2){300, 250};
    outer_wall.points[9] = (Vec2){50, 250};
    outer_wall.count = 10;

    // Inner Island 1 (Top Left block)
    inner_wall.points[0] = (Vec2){150, 150};
    inner_wall.points[1] = (Vec2){250, 150};
    inner_wall.points[2] = (Vec2){250, 200};
    inner_wall.points[3] = (Vec2){150, 200};
    inner_wall.count = 4; // Note: we'll handle multiple islands by drawing lines manually
}

// Custom Wall Logic: Check collision against a list of line segments
void CheckWallCollision(Car *c, Vec2 pA, Vec2 pB) {
    Vec2 ab = v_sub(pB, pA);
    Vec2 ap = v_sub(c->pos, pA);
    
    float t = v_dot(ap, ab) / v_dot(ab, ab);
    
    // Clamp t to segment [0, 1]
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    
    Vec2 closest = v_add(pA, v_mul(ab, t));
    Vec2 diff = v_sub(c->pos, closest);
    float dist = v_len(diff);
    
    if (dist < c->radius && dist > 0) {
        // Collision Normal
        Vec2 n = v_norm(diff);
        
        // Push out
        float overlap = c->radius - dist;
        c->pos = v_add(c->pos, v_mul(n, overlap));
        
        // Bounce velocity
        float v_dot_n = v_dot(c->vel, n);
        if (v_dot_n < 0) {
            Vec2 reflect = v_mul(n, 2 * v_dot_n);
            c->vel = v_sub(c->vel, reflect);
            c->vel = v_mul(c->vel, WALL_BOUNCE); // Lose energy
        }
    }
}

void InitGame() {
    InitTrack();
    p1.pos = (Vec2){100, 350};
    p1.vel = (Vec2){0, 0};
    p1.angle = -PI / 2; // Facing up
    p1.radius = 10.0f;
    p1.color = RGB(255, 50, 50);
}

void UpdatePhysics() {
    // Steering
    if (key_left) p1.angle -= TURN_SPEED;
    if (key_right) p1.angle += TURN_SPEED;

    // Acceleration Vector (Where the car is pointing)
    Vec2 dir = {cosf(p1.angle), sinf(p1.angle)};

    // Gas
    if (key_up) {
        p1.vel = v_add(p1.vel, v_mul(dir, ACCELERATION));
    }
    if (key_down) {
        p1.vel = v_sub(p1.vel, v_mul(dir, ACCELERATION * 0.5f));
    }

    // Apply Drag (Air resistance)
    p1.vel = v_mul(p1.vel, DRAG);

    // Apply Drift (Lateral Friction)
    // Project velocity onto forward direction and right direction
    Vec2 forward = dir;
    Vec2 right = {-dir.y, dir.x};
    
    float fwd_speed = v_dot(p1.vel, forward);
    float side_speed = v_dot(p1.vel, right);
    
    // Reduce side speed (kill drift over time)
    side_speed *= DRIFT_FACTOR;
    
    // Reassemble velocity
    p1.vel = v_add(v_mul(forward, fwd_speed), v_mul(right, side_speed));

    // Cap max speed
    if (v_len(p1.vel) > MAX_SPEED) {
        p1.vel = v_mul(v_norm(p1.vel), MAX_SPEED);
    }

    // Move
    p1.pos = v_add(p1.pos, p1.vel);

    // --- Collisions ---
    
    // Define track geometry on the fly for simplicity
    // Outer box
    Vec2 walls[] = {
        {20, 20}, {760, 20}, 
        {760, 20}, {760, 540},
        {760, 540}, {20, 540},
        {20, 540}, {20, 20},
        // Inner Island 1
        {150, 150}, {300, 150},
        {300, 150}, {300, 400},
        {300, 400}, {150, 400},
        {150, 400}, {150, 150},
        // Inner Island 2
        {460, 150}, {610, 150},
        {610, 150}, {610, 400},
        {610, 400}, {460, 400},
        {460, 400}, {460, 150}
    };

    for (int i = 0; i < sizeof(walls)/sizeof(Vec2); i+=2) {
        CheckWallCollision(&p1, walls[i], walls[i+1]);
    }
}

// --- Rendering ---

void DrawCar(HDC hdc, Car c) {
    // Create a rotated polygon for the car
    POINT pts[4];
    float w = 8.0f;  // Half width
    float l = 14.0f; // Half length
    
    // Model space coordinates relative to car center (facing right)
    // Shape: slightly tapered front like an F1 car
    Vec2 model[] = {
        {l, 0}, {-l, -w}, {-l+4, 0}, {-l, w} 
    };

    float cosA = cosf(c.angle);
    float sinA = sinf(c.angle);

    for(int i=0; i<4; i++) {
        // Rotate
        float rx = model[i].x * cosA - model[i].y * sinA;
        float ry = model[i].x * sinA + model[i].y * cosA;
        // Translate
        pts[i].x = (LONG)(c.pos.x + rx);
        pts[i].y = (LONG)(c.pos.y + ry);
    }

    // Draw
    HBRUSH brush = CreateSolidBrush(c.color);
    HBRUSH oldBrush = SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HPEN oldPen = SelectObject(hdc, pen);

    Polygon(hdc, pts, 4);

    // Draw simple spoiler/wing
    MoveToEx(hdc, pts[1].x, pts[1].y, NULL);
    LineTo(hdc, pts[3].x, pts[3].y);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawTrack(HDC hdc) {
    // Fill Background
    RECT rect = {0, 0, SCREEN_W, SCREEN_H};
    HBRUSH grass = CreateSolidBrush(RGB(30, 160, 30));
    FillRect(hdc, &rect, grass);
    DeleteObject(grass);

    // Draw Asphalt (We cheat by drawing thick grey lines for the track path, or filling poly)
    // For this demo, we will just draw the walls and let the "grass" be the track surface 
    // for simplicity, or reverse it. Let's make the background Grey (Asphalt) and draw Green Islands.
    
    HBRUSH asphalt = CreateSolidBrush(RGB(80, 80, 80));
    FillRect(hdc, &rect, asphalt); // Whole screen is road
    DeleteObject(asphalt);

    // Draw Walls (Barriers)
    HPEN wallPen = CreatePen(PS_SOLID, 5, RGB(200, 200, 200)); // White walls
    HPEN oldPen = SelectObject(hdc, wallPen);

    // Draw geometry defined in UpdatePhysics for visual consistency
    Vec2 walls[] = {
        {20, 20}, {760, 20}, 
        {760, 20}, {760, 540},
        {760, 540}, {20, 540},
        {20, 540}, {20, 20},
        // Inner Island 1
        {150, 150}, {300, 150},
        {300, 150}, {300, 400},
        {300, 400}, {150, 400},
        {150, 400}, {150, 150},
        // Inner Island 2
        {460, 150}, {610, 150},
        {610, 150}, {610, 400},
        {610, 400}, {460, 400},
        {460, 400}, {460, 150}
    };

    // Draw Grass Islands inside the barriers
    HBRUSH islandBrush = CreateSolidBrush(RGB(30, 160, 30));
    SelectObject(hdc, islandBrush);
    
    // Outer Border (Inverted logic: Draw 4 Rects to frame the screen)
    // Top
    Rectangle(hdc, 0, 0, SCREEN_W, 20);
    // Bottom
    Rectangle(hdc, 0, 540, SCREEN_W, SCREEN_H);
    // Left
    Rectangle(hdc, 0, 0, 20, SCREEN_H);
    // Right
    Rectangle(hdc, 760, 0, SCREEN_W, SCREEN_H);

    // Island 1
    Rectangle(hdc, 150, 150, 300, 400);
    // Island 2
    Rectangle(hdc, 460, 150, 610, 400);

    SelectObject(hdc, oldPen);
    DeleteObject(wallPen);
    DeleteObject(islandBrush);

    // UI
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, 10, 10, "SUPER C-SPRINT: Arrows to Drive", 31);
}

// --- Windows Boilerplate ---

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            game_running = false;
            PostQuitMessage(0);
            return 0;
        
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:    key_up = true; break;
                case VK_DOWN:  key_down = true; break;
                case VK_LEFT:  key_left = true; break;
                case VK_RIGHT: key_right = true; break;
                case VK_ESCAPE: game_running = false; break;
            }
            return 0;

        case WM_KEYUP:
            switch (wParam) {
                case VK_UP:    key_up = false; break;
                case VK_DOWN:  key_down = false; break;
                case VK_LEFT:  key_left = false; break;
                case VK_RIGHT: key_right = false; break;
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "RacerWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Calculate window size to fit client area exactly
    RECT winRect = {0, 0, SCREEN_W, SCREEN_H};
    AdjustWindowRect(&winRect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Super C-Sprint",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        winRect.right - winRect.left, winRect.bottom - winRect.top,
        NULL, NULL, hInstance, NULL
    );

    InitGame();

    // Double Buffering Setup
    HDC hdc = GetDC(hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
    SelectObject(memDC, memBitmap);

    // Timing
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    double target_frame_time = 1.0 / FPS;

    // Main Loop
    MSG msg = {0};
    while (game_running) {
        QueryPerformanceCounter(&start);

        // Handle Windows Messages (Input)
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) game_running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Game Logic
        UpdatePhysics();

        // Render
        DrawTrack(memDC);
        DrawCar(memDC, p1);

        // Flip Buffer
        BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, memDC, 0, 0, SRCCOPY);

        // Cap FPS
        QueryPerformanceCounter(&end);
        double elapsed = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed < target_frame_time) {
            Sleep((DWORD)((target_frame_time - elapsed) * 1000));
        }
    }

    // Cleanup
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);

    return 0;
}
