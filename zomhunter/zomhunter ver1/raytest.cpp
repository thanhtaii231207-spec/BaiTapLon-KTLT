#include "raylib.h"

int main()
{
    InitWindow(800, 600, "Raylib Test");

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(DARKGREEN);

        DrawCircle(400, 300, 50, BLUE);

        DrawText("Raylib da hoat dong!", 220, 100, 30, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}