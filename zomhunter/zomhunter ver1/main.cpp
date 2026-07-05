#include "raylib.h"
#include <vector>
#include <cmath>

struct Enemy
{
    Vector2 pos;
    int hp;
};

int main()
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Bush Battle");
    SetTargetFPS(60);

    Vector2 playerPos = {500, 500};
    int playerHP = 100;

    std::vector<Enemy> enemies =
    {
        {{800, 600},100},
        {{1200, 700},100},
        {{900, 1200},100},
        {{1500, 900},100}
    };

    std::vector<Vector2> bushes;

    for(int i=0;i<120;i++)
    {
        bushes.push_back({
            (float)GetRandomValue(0,3000),
            (float)GetRandomValue(0,3000)
        });
    }

    Camera2D camera = {0};
    camera.zoom = 1.0f;

    while(!WindowShouldClose())
    {
        float speed = 250 * GetFrameTime();

        if(IsKeyDown(KEY_W)) playerPos.y -= speed;
        if(IsKeyDown(KEY_S)) playerPos.y += speed;
        if(IsKeyDown(KEY_A)) playerPos.x -= speed;
        if(IsKeyDown(KEY_D)) playerPos.x += speed;

        camera.target = playerPos;
        camera.offset = {
            (float)screenWidth/2,
            (float)screenHeight/2
        };

        for(auto &e : enemies)
        {
            if(e.hp <= 0) continue;

            Vector2 dir =
            {
                playerPos.x - e.pos.x,
                playerPos.y - e.pos.y
            };

            float len = sqrt(dir.x*dir.x + dir.y*dir.y);

            if(len > 0)
            {
                dir.x /= len;
                dir.y /= len;

                e.pos.x += dir.x * 80 * GetFrameTime();
                e.pos.y += dir.y * 80 * GetFrameTime();
            }

            if(len < 30)
            {
                playerHP--;
            }
        }

        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode2D(camera);

        DrawRectangle(0,0,3000,3000,DARKGREEN);

        for(auto &b : bushes)
        {
            DrawCircleV(b,25,GREEN);
        }

        DrawCircleV(playerPos,20,BLUE);

        for(auto &e : enemies)
        {
            if(e.hp > 0)
            {
                DrawCircleV(e.pos,20,RED);
            }
        }

        EndMode2D();

        DrawText(
            TextFormat("HP: %d", playerHP),
            20,
            20,
            30,
            WHITE
        );

        DrawText(
            "WASD Move",
            20,
            60,
            20,
            WHITE
        );

        EndDrawing();
    }

    CloseWindow();
    return 0;
}