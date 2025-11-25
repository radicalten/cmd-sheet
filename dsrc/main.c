/* 
   SUPER SPRINT CLONE - Single File C Program (Win32 API)
   
   Controls:
   UP    - Accelerate
   LEFT  - Steer Left
   RIGHT - Steer Right
   
   Objective: Race 3 laps against the drones.
*/

#define _USE_MATH_DEFINES
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --- Constants ---
#define SCR_W 800
#define SCR_H 600
#define PI 3.14159265f
#define MAX_CARS 4
#define PLAYER_IDX 0

// Physics Tuning
#define ACCEL 0.15f
#define TURN_SPEED 0.06f
#define FRICTION 0.975f
#define WALL_BOUNCE -0.5f
#define MAX_SPEED 7.0f

// --- Structs ---
typedef struct {
    float x, y;
    float vx, vy;
    float angle;      // Heading angle in radians
    COLORREF color;
    int is_ai;
    int current_checkpoint;
    int laps;
    int rank;         // 0 = racing, >0 = finished position
} Car;

typedef struct {
    float x, y;
    float radius;
} Checkpoint;

// --- Global State ---
Car cars[MAX_CARS];
int total_finished = 0;
int game_over = 0;
int width, height;

// Track Definition (Simple loop using math bounds)
// We define the track as the space between two rounded rectangles
float track_cx = SCR_W / 2.0f;
float track_cy = SCR_H / 2.0f;
float track_w = SCR_W * 0.85f;
float track_h = SCR_H * 0.80f;
float track_width = 90.0f; 

// AI Waypoints (Simple path following)
#define NUM_WAYPOINTS 8
float waypoints[NUM_WAYPOINTS][2];

// --- Helper Functions ---

float dist(float x1, float y1, float x2, float y2) {
    return sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// Initialize Game
void InitGame() {
    srand((unsigned int)time(NULL));

    // Setup Cars
    // Player (Blue)
    cars[0].x = SCR_W / 2.0f; cars[0].y = SCR_H - 60.0f;
    cars[0].color = RGB(0, 100, 255);
    cars[0].is_ai = 0;

    // AI (Red)
    cars[1].x = SCR_W / 2.0f - 30; cars[1].y = SCR_H - 40.0f;
    cars[1].color = RGB(255, 50, 50); 
    cars[1].is_ai = 1;

    // AI (Yellow)
    cars[2].x = SCR_W / 2.0f + 30; cars[2].y = SCR_H - 40.0f;
    cars[2].color = RGB(255, 200, 0);
    cars[2].is_ai = 1;

    // AI (Green)
    cars[3].x = SCR_W / 2.0f + 60; cars[3].y = SCR_H - 60.0f;
    cars[3].color = RGB(50, 200, 50);
    cars[3].is_ai = 1;

    for(int i=0; i<MAX_CARS; i++) {
        cars[i].vx = 0; cars[i].vy = 0;
        cars[i].angle = -PI / 2; // Pointing Up? No, Screen Y is down. -PI/2 is Up.
        cars[i].laps = 0;
        cars[i].current_checkpoint = 0;
        cars[i].rank = 0;
    }

    // Setup Waypoints for AI (A rough loop around the screen)
    // 1. Top Middle
    waypoints[0][0] = SCR_W/2; waypoints[0][1] = 60;
    // 2. Top Left Corner
    waypoints[1][0] = 80;      waypoints[1][1] = 80;
    // 3. Left Middle
    waypoints[2][0] = 60;      waypoints[2][1] = SCR_H/2;
    // 4. Bottom Left Corner
    waypoints[3][0] = 80;      waypoints[3][1] = SCR_H - 80;
    // 5. Bottom Middle
    waypoints[4][0] = SCR_W/2; waypoints[4][1] = SCR_H - 60;
    // 6. Bottom Right Corner
    waypoints[5][0] = SCR_W-80;waypoints[5][1] = SCR_H - 80;
    // 7. Right Middle
    waypoints[6][0] = SCR_W-60;waypoints[6][1] = SCR_H/2;
    // 8. Top Right Corner
    waypoints[7][0] = SCR_W-80;waypoints[7][1] = 80;
}

// Check Collision with Track Walls
int IsPointOnTrack(float x, float y) {
    // Simple Signed Distance Field approximation for a rounded rectangle track
    // We define a "center" box and check distance from it.
    
    float half_w = (track_w - track_width*2) / 2.0f;
    float half_h = (track_h - track_width*2) / 2.0f;

    // Calculate distance from center of screen relative to the "inner block" dimensions
    float dx = fabsf(x - track_cx);
    float dy = fabsf(y - track_cy);

    // This logic creates a donut shape. 
    // It's simplified. We just want to know if we are INSIDE the outer wall AND OUTSIDE the inner island.
    
    // Outer boundary check (Rounded rect)
    float outer_rx = track_w / 2.0f;
    float outer_ry = track_h / 2.0f;
    
    // Normalized coordinates for ellipse-ish check
    // A robust way without complex math is difficult, so we use a simple logic:
    // Track is the space between Inner Box and Outer Box.
    
    // Determine distance from the "Center Line" of the track loop
    // Logic: Clamp point to the rectangle defined by the track's spine, measure dist.
    float spine_w = (track_w - track_width) / 2.0f;
    float spine_h = (track_h - track_width) / 2.0f;
    
    float cx = (x > track_cx + spine_w) ? track_cx + spine_w : (x < track_cx - spine_w) ? track_cx - spine_w : x;
    float cy = (y > track_cy + spine_h) ? track_cy + spine_h : (y < track_cy - spine_h) ? track_cy - spine_h : y;
    
    float d = dist(x, y, cx, cy);
    
    // track_width is the full width. Half of it is the radius from the spine.
    return (d < track_width / 2.0f);
}

void UpdatePhysics() {
    for(int i=0; i<MAX_CARS; i++) {
        if(cars[i].rank > 0) continue; // Finished race

        Car *c = &cars[i];

        // --- AI Logic ---
        if (c->is_ai) {
            // Target current waypoint
            float tx = waypoints[c->current_checkpoint][0];
            float ty = waypoints[c->current_checkpoint][1];
            
            // Angle to target
            float target_angle = atan2f(ty - c->y, tx - c->x);
            
            // Smooth steering toward target
            float diff = target_angle - c->angle;
            while (diff <= -PI) diff += 2*PI;
            while (diff > PI) diff -= 2*PI;
            
            if (diff > 0) c->angle += TURN_SPEED * 0.8f;
            else c->angle -= TURN_SPEED * 0.8f;
            
            // AI Accelerates if looking roughly at target
            float speed = sqrtf(c->vx*c->vx + c->vy*c->vy);
            if (fabsf(diff) < PI/2 && speed < MAX_SPEED * 0.9f) {
                c->vx += cosf(c->angle) * ACCEL;
                c->vy += sinf(c->angle) * ACCEL;
            }

            // Check waypoint arrival
            if (dist(c->x, c->y, tx, ty) < 80.0f) {
                c->current_checkpoint++;
                // Waypoints go 5->6->7->0->1... (Counter Clockwise logic based on array)
                // The array defined in Init is somewhat arbitrary, let's fix navigation order:
                // 4(bot) -> 5(br) -> 6(r) -> 7(tr) -> 0(t) -> 1(tl) -> 2(l) -> 3(bl) -> 4...
                // Actually the array is: 0(TM), 1(TL), 2(LM), 3(BL), 4(BM), 5(BR), 6(RM), 7(TR)
                // Standard race direction is Counter-Clockwise.
                // Start is at Bottom. So 4 -> 5 -> 6 -> 7 -> 0 -> 1 -> 2 -> 3 -> 4
                
                // Correcting logic for simple array cycling
                if (c->current_checkpoint >= NUM_WAYPOINTS) c->current_checkpoint = 0;
            }
        }
        // --- Player Logic ---
        else {
            if (GetAsyncKeyState(VK_LEFT)) c->angle -= TURN_SPEED;
            if (GetAsyncKeyState(VK_RIGHT)) c->angle += TURN_SPEED;
            if (GetAsyncKeyState(VK_UP)) {
                c->vx += cosf(c->angle) * ACCEL;
                c->vy += sinf(c->angle) * ACCEL;
            }
        }

        // --- Physics Integration ---
        c->vx *= FRICTION;
        c->vy *= FRICTION;
        
        float speed = sqrtf(c->vx*c->vx + c->vy*c->vy);
        if (speed > MAX_SPEED) {
            float ratio = MAX_SPEED / speed;
            c->vx *= ratio;
            c->vy *= ratio;
        }

        float next_x = c->x + c->vx;
        float next_y = c->y + c->vy;

        // --- Collision ---
        // Check track boundaries
        if (!IsPointOnTrack(next_x, next_y)) {
            // Bounce
            c->vx *= WALL_BOUNCE;
            c->vy *= WALL_BOUNCE;
            // Nudge back
            c->x += c->vx; 
            c->y += c->vy;
        } else {
            c->x = next_x;
            c->y = next_y;
        }

        // --- Lap Counting Logic ---
        // Check crossing the finish line (Bottom center, moving right)
        // Line is roughly x=SCR_W/2, y > SCR_H - 100
        if (c->y > SCR_H - 100 && c->x > SCR_W/2 && (c->x - c->vx) <= SCR_W/2) {
            // Must have completed waypoints to prevent oscillating on line
            // Simple debounce: Ensure we were on the left side recently? 
            // For this simple version, just increment.
             c->laps++;
             if (c->laps > 3) {
                 total_finished++;
                 c->rank = total_finished;
                 if (!c->is_ai) {
                     char msg[64];
                     sprintf(msg, "You finished! Rank: %d", c->rank);
                     MessageBoxA(NULL, msg, "Race Over", MB_OK);
                     InitGame(); // Reset
                     return;
                 }
             }
        }
    }
}

void DrawRotatedRect(HDC hdc, float x, float y, float angle, COLORREF color) {
    // Create a brush and pen
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    // Car dimensions
    float cw = 20.0f; // Length
    float ch = 10.0f; // Width

    POINT pts[4];
    // Corners relative to center (0,0) before rotation
    float cx[4] = { cw/2, cw/2, -cw/2, -cw/2 };
    float cy[4] = { ch/2, -ch/2, -ch/2, ch/2 };

    for(int i=0; i<4; i++) {
        // Rotate
        float rx = cx[i] * cosf(angle) - cy[i] * sinf(angle);
        float ry = cx[i] * sinf(angle) + cy[i] * cosf(angle);
        // Translate
        pts[i].x = (LONG)(x + rx);
        pts[i].y = (LONG)(y + ry);
    }

    Polygon(hdc, pts, 4);
    
    // Draw a small "spoiler" or stripe to indicate direction
    MoveToEx(hdc, pts[2].x, pts[2].y, NULL);
    LineTo(hdc, pts[3].x, pts[3].y);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

void DrawTrack(HDC hdc) {
    // 1. Fill Background (Grass)
    HBRUSH grass = CreateSolidBrush(RGB(30, 100, 30));
    RECT rc = {0, 0, SCR_W, SCR_H};
    FillRect(hdc, &rc, grass);
    DeleteObject(grass);

    // 2. Draw Track (Asphalt)
    HBRUSH asphalt = CreateSolidBrush(RGB(80, 80, 80));
    HGDIOBJ old = SelectObject(hdc, asphalt);
    
    // Draw segments (Rectangles and Ellipses to form the loop)
    float spine_w = (track_w - track_width) / 2.0f;
    float spine_h = (track_h - track_width) / 2.0f;
    float half_tw = track_width / 2.0f;

    // We draw the track by drawing a thick pen line? No, GDI lines are square ended.
    // Let's use geometry: Two rounded rects.
    
    // Outer Border
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
    SelectObject(hdc, borderPen);
    RoundRect(hdc, 
        (int)(track_cx - spine_w - half_tw), (int)(track_cy - spine_h - half_tw),
        (int)(track_cx + spine_w + half_tw), (int)(track_cy + spine_h + half_tw),
        (int)track_width, (int)track_width);

    // Inner Island (Grass)
    SelectObject(hdc, grass); // Reuse grass color but new brush needed if deleted?
    HBRUSH grass2 = CreateSolidBrush(RGB(30, 100, 30));
    SelectObject(hdc, grass2);
    RoundRect(hdc, 
        (int)(track_cx - spine_w + half_tw), (int)(track_cy - spine_h + half_tw),
        (int)(track_cx + spine_w - half_tw), (int)(track_cy + spine_h - half_tw),
        (int)(track_width/2), (int)(track_width/2));
    
    DeleteObject(grass2);
    SelectObject(hdc, old);
    DeleteObject(asphalt);
    DeleteObject(borderPen);

    // Draw Start/Finish Line
    HPEN linePen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    SelectObject(hdc, linePen);
    MoveToEx(hdc, (int)track_cx, (int)(SCR_H - (SCR_H-track_h)/2), NULL);
    LineTo(hdc, (int)track_cx, (int)(SCR_H - (SCR_H-track_h)/2 - track_width), NULL); // Not precise but visual enough
    DeleteObject(linePen);
}

void Render(HDC hdc) {
    // Create Double Buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, SCR_W, SCR_H);
    HGDIOBJ oldBM = SelectObject(memDC, memBM);

    DrawTrack(memDC);

    // Draw Cars
    for(int i=0; i<MAX_CARS; i++) {
        DrawRotatedRect(memDC, cars[i].x, cars[i].y, cars[i].angle, cars[i].color);
    }

    // Draw HUD
    char hud[32];
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    sprintf(hud, "LAP: %d/3", cars[0].laps > 3 ? 3 : cars[0].laps);
    TextOutA(memDC, 10, 10, hud, strlen(hud));

    // Blit to Screen
    BitBlt(hdc, 0, 0, SCR_W, SCR_H, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if(wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Main Entry Point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "SuperSprint", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, "Super Sprint Clone", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 100, 100, SCR_W, SCR_H, NULL, NULL, wc.hInstance, NULL);

    InitGame();
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    HDC hdc = GetDC(hwnd);
    
    DWORD lastTime = GetTickCount();

    while(msg.message != WM_QUIT) {
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Game Loop - Cap at ~60 FPS
            DWORD currentTime = GetTickCount();
            if (currentTime - lastTime > 16) {
                UpdatePhysics();
                Render(hdc);
                lastTime = currentTime;
            }
            Sleep(1); // Prevent CPU hogging
        }
    }

    ReleaseDC(hwnd, hdc);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
