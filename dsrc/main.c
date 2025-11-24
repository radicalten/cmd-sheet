/*
 * SUPER OFF-ROAD TRIBUTE - Single File C Program
 * Platform: Windows (Win32 API)
 *
 * Controls:
 *   ARROW KEYS: Steer and Accelerate
 *   SPACE:      Nitro
 *   ESC:        Exit
 */

#include <windows.h>
#include <math.h>
#include <stdio.h>

#define SCREEN_W 800
#define SCREEN_H 600
#define PI 3.14159265f
#define DEG2RAD(x) ((x) * (PI / 180.0f))

// --- Physics Constants ---
#define ACCEL 0.25f
#define TURN_SPEED 4.5f
#define DRAG 0.97f          // Friction (loose dirt feel)
#define OFFROAD_DRAG 0.90f  // Friction when hitting walls/grass
#define MAX_SPEED 12.0f
#define NITRO_FORCE 0.6f
#define CAR_WIDTH 14
#define CAR_LENGTH 24

// --- Game State ---
typedef struct {
    float x, y;
    float vx, vy;
    float angle; // Degrees
    int nitro_fuel;
    COLORREF color;
} Car;

Car player;
int game_running = 1;
int width, height;

// Track boundaries (Two islands for Figure-8)
// Island 1 (Left)
RECT island1 = { 150, 150, 350, 450 };
// Island 2 (Right)
RECT island2 = { 450, 150, 650, 450 };

// --- Function Prototypes ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitGame();
void UpdatePhysics();
void DrawGame(HDC hdc, RECT* rect);
void DrawRotatedRect(HDC hdc, float x, float y, float w, float h, float angle, COLORREF color);

// --- Entry Point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "OffRoadClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Adjust window size to ensure client area is SCREEN_W x SCREEN_H
    RECT winRect = { 0, 0, SCREEN_W, SCREEN_H };
    AdjustWindowRect(&winRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Single File Super Off-Road Tribute",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winRect.right - winRect.left, winRect.bottom - winRect.top,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    InitGame();

    // Main Game Loop
    MSG msg = {0};
    HDC hdc;
    LARGE_INTEGER frequency, start, end;
    double interval;
    
    QueryPerformanceFrequency(&frequency);
    interval = 1000.0 / 60.0; // 60 FPS target

    while (game_running) {
        // Handle Windows messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) game_running = 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        QueryPerformanceCounter(&start);

        // Game Logic
        UpdatePhysics();

        // Render
        hdc = GetDC(hwnd);
        GetClientRect(hwnd, &winRect);
        DrawGame(hdc, &winRect);
        ReleaseDC(hwnd, hdc);

        // Frame Limiter
        QueryPerformanceCounter(&end);
        double elapsed = (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
        if (elapsed < interval) {
            Sleep((DWORD)(interval - elapsed));
        }
    }

    return 0;
}

void InitGame() {
    player.x = 400;
    player.y = 520;
    player.vx = 0;
    player.vy = 0;
    player.angle = 0; // Facing right
    player.nitro_fuel = 100;
    player.color = RGB(255, 50, 50); // Red Truck
}

// Check if point is inside a rectangle
int PointInRect(float x, float y, RECT r) {
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

void UpdatePhysics() {
    if (GetAsyncKeyState(VK_ESCAPE)) game_running = 0;

    // Steering
    if (GetAsyncKeyState(VK_LEFT))  player.angle -= TURN_SPEED;
    if (GetAsyncKeyState(VK_RIGHT)) player.angle += TURN_SPEED;

    // Acceleration calculation
    float rad = DEG2RAD(player.angle);
    float dirX = cos(rad);
    float dirY = sin(rad);

    float current_accel = 0;

    if (GetAsyncKeyState(VK_UP)) current_accel = ACCEL;
    if (GetAsyncKeyState(VK_DOWN)) current_accel = -ACCEL * 0.5f; // Reverse is slower

    // Nitro
    if (GetAsyncKeyState(VK_SPACE) && player.nitro_fuel > 0) {
        current_accel += NITRO_FORCE;
        player.nitro_fuel--;
    }

    // Apply force to velocity
    player.vx += dirX * current_accel;
    player.vy += dirY * current_accel;

    // Apply Drag (Friction)
    // This creates the "sliding" feel. The car keeps moving in previous vector 
    // even if the car rotates, until friction and new acceleration take over.
    player.vx *= DRAG;
    player.vy *= DRAG;

    // Cap Speed
    float speed = sqrt(player.vx*player.vx + player.vy*player.vy);
    if (speed > MAX_SPEED) {
        float scale = MAX_SPEED / speed;
        player.vx *= scale;
        player.vy *= scale;
    }

    // Update Position
    float nextX = player.x + player.vx;
    float nextY = player.y + player.vy;

    // Collision Detection (Simple Bounding Box bounce)
    int collision = 0;

    // Outer Walls
    if (nextX < 20 || nextX > SCREEN_W - 20) { player.vx *= -0.5f; collision = 1; }
    if (nextY < 20 || nextY > SCREEN_H - 20) { player.vy *= -0.5f; collision = 1; }

    // Inner Islands (The Figure 8 Holes)
    // Slightly padded to give walls thickness
    RECT r1 = island1; InflateRect(&r1, 10, 10);
    RECT r2 = island2; InflateRect(&r2, 10, 10);

    if (PointInRect(nextX, nextY, r1) || PointInRect(nextX, nextY, r2)) {
        // Determine bounce direction (very simple approximation)
        // If we are deep inside, just reverse velocity to prevent sticking
        player.vx *= -0.6f;
        player.vy *= -0.6f;
        collision = 1;
    }

    if (!collision) {
        player.x = nextX;
        player.y = nextY;
    }
}

void DrawRotatedRect(HDC hdc, float x, float y, float w, float h, float angle, COLORREF color) {
    // Create points relative to center
    float rad = DEG2RAD(angle);
    float c = cos(rad);
    float s = sin(rad);
    
    POINT pts[4];
    float hw = w / 2.0f;
    float hh = h / 2.0f;

    // Corners relative to center
    float cx[4] = { -hw, hw, hw, -hw };
    float cy[4] = { -hh, -hh, hh, hh };

    for (int i = 0; i < 4; i++) {
        pts[i].x = (LONG)(x + (cx[i] * c - cy[i] * s));
        pts[i].y = (LONG)(y + (cx[i] * s + cy[i] * c));
    }

    HBRUSH hBrush = CreateSolidBrush(color);
    HBRUSH oldBrush = SelectObject(hdc, hBrush);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HPEN oldPen = SelectObject(hdc, hPen);

    Polygon(hdc, pts, 4);

    // Draw a windshield to indicate direction
    POINT front[3];
    front[0] = pts[1]; // Front Right
    front[1] = pts[2]; // Front Left
    // Center point slightly back
    front[2].x = (LONG)(x + ((hw-6) * c - 0 * s)); 
    front[2].y = (LONG)(y + ((hw-6) * s + 0 * c));
    
    HBRUSH winBrush = CreateSolidBrush(RGB(200, 240, 255));
    SelectObject(hdc, winBrush);
    Polygon(hdc, front, 3);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hBrush);
    DeleteObject(winBrush);
    DeleteObject(hPen);
}

void DrawGame(HDC hdc, RECT* clientRect) {
    // Create Double Buffer to prevent flickering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect->right, clientRect->bottom);
    HBITMAP oldBitmap = SelectObject(memDC, memBitmap);

    // --- Draw Background (Dirt) ---
    HBRUSH dirtBrush = CreateSolidBrush(RGB(139, 69, 19)); // Brown
    FillRect(memDC, clientRect, dirtBrush);
    DeleteObject(dirtBrush);

    // --- Draw Track Borders (Walls/Grass) ---
    HBRUSH grassBrush = CreateSolidBrush(RGB(34, 139, 34)); // Forest Green
    
    // 1. Outer Boundary (Fill screen with grass, then draw dirt track on top? 
    //    No, easier to draw dirt background then draw grass obstacles)
    
    // Draw border walls (Grass)
    int border = 20;
    RECT top = {0, 0, SCREEN_W, border};
    RECT bot = {0, SCREEN_H-border, SCREEN_W, SCREEN_H};
    RECT left = {0, 0, border, SCREEN_H};
    RECT right = {SCREEN_W-border, 0, SCREEN_W, SCREEN_H};
    
    FillRect(memDC, &top, grassBrush);
    FillRect(memDC, &bot, grassBrush);
    FillRect(memDC, &left, grassBrush);
    FillRect(memDC, &right, grassBrush);

    // Draw Inner Islands (The Figure 8 shape)
    FillRect(memDC, &island1, grassBrush);
    FillRect(memDC, &island2, grassBrush);

    // Draw Bumps / Haybales (Visual flair)
    HBRUSH whiteBrush = CreateSolidBrush(RGB(220, 220, 220));
    HBRUSH redBrush = CreateSolidBrush(RGB(200, 0, 0));
    
    RECT obstacles[] = { island1, island2 };
    for(int k=0; k<2; k++) {
        FrameRect(memDC, &obstacles[k], whiteBrush); // Simple outline
    }

    DeleteObject(grassBrush);
    DeleteObject(whiteBrush);
    DeleteObject(redBrush);

    // --- Draw Start / Finish Line ---
    // Bottom center
    HBRUSH check1 = CreateSolidBrush(RGB(255,255,255));
    HBRUSH check2 = CreateSolidBrush(RGB(0,0,0));
    int startX = 380, startY = 480;
    for(int i=0; i<4; i++) {
        for(int j=0; j<2; j++) {
            RECT c = {startX + i*10, startY + j*10, startX + (i+1)*10, startY + (j+1)*10};
            FillRect(memDC, &c, ((i+j)%2) ? check1 : check2);
        }
    }
    DeleteObject(check1);
    DeleteObject(check2);

    // --- Draw Shadow ---
    DrawRotatedRect(memDC, player.x + 5, player.y + 5, CAR_LENGTH, CAR_WIDTH, player.angle, RGB(50, 20, 0));
    
    // --- Draw Player Car ---
    DrawRotatedRect(memDC, player.x, player.y, CAR_LENGTH, CAR_WIDTH, player.angle, player.color);

    // --- Draw UI ---
    char buf[64];
    sprintf(buf, "NITRO: %d", player.nitro_fuel);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 0));
    TextOut(memDC, 30, 30, buf, (int)strlen(buf));
    
    sprintf(buf, "WASD / ARROWS to Drive | SPACE for Nitro");
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOut(memDC, 30, SCREEN_H - 50, buf, (int)strlen(buf));

    // Copy buffer to screen
    BitBlt(hdc, 0, 0, clientRect->right, clientRect->bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            game_running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1; // Prevent flickering
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
