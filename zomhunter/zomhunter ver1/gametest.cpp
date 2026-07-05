#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include "raylib.h"
using std::clamp;

const int SCREEN_W    = 1280;
const int SCREEN_H    = 720;
const int MAP_W       = 3200;
const int MAP_H       = 2400;
const float DAY_DUR   = 30.0f;
const int EXPLOSION_FRAMES = 8;

struct SpriteAssets
{
    Texture2D player;
    Texture2D zombie;
    Texture2D bullet;
    Texture2D bomb;
    Texture2D ground;
    Texture2D explosion;   
    bool hasPlayer;
    bool hasZombie;
    bool hasBullet;
    bool hasBomb;
    bool hasGround;
    bool hasExplosion;
};

struct Player
{
    Vector2 pos;
    float   hp, maxHp;
    int     bombs;
    float   shootCooldown;
    float   invincible;
    float   angle;          // hướng nhìn (độ)
    float   walkAnim;       // 0..1, dùng để bob sprite
};

struct Zombie
{
    Vector2 pos;
    float   hp, maxHp;
    float   speed, damage;
    int     tier;
    float   flashTimer;
    bool    alive;
    float   angle;
    float   walkAnim;
};

struct Bullet
{
    Vector2 pos, vel;
    float   life;
    bool    active;
    float   angle;
};

struct Bomb
{
    Vector2 pos;
    float   radius, timer;
    bool    exploded, done;
    int     frame;
    float   frameTimer;
};

struct Particle
{
    Vector2 pos, vel;
    Color   color;
    float   life, size;
};

enum GameState { MENU, PLAYING, GAMEOVER };

struct Game
{
    GameState state;
    Player    player;
    std::vector<Zombie>   zombies;
    std::vector<Bullet>   bullets;
    std::vector<Bomb>     bombs;
    std::vector<Particle> particles;
    int   day, finalDay, score;
    float dayTimer, spawnTimer;
};

// ─── Helpers ─────────────────────────────────────────────────────────────────
float VLen(Vector2 v){ return sqrtf(v.x*v.x+v.y*v.y); }
Vector2 VNorm(Vector2 v){ float l=VLen(v); return l>0?(Vector2){v.x/l,v.y/l}:(Vector2){0,0}; }

float ZombieHP(int d)    { return 80.0f  + (d/10)*60.0f; }
float ZombieSpeed(int d) { return 60.0f  + (d/10)*20.0f; }
float ZombieDmg(int d)   { return 0.35f  + (d/10)*0.15f; }
int   SpawnCount(int d)  { return 6 + d*2; }

void AddParticles(Game &g, Vector2 pos, Color col, int n, float spd=120.f, float sz=4.f)
{
    for(int i=0;i<n;i++){
        float a=(float)GetRandomValue(0,360)*DEG2RAD;
        float s=(float)GetRandomValue(40,(int)spd);
        g.particles.push_back({pos,{cosf(a)*s,sinf(a)*s},col,1.f,sz});
    }
}

void SpawnZombie(Game &g)
{
    Vector2 pos;
    int side=GetRandomValue(0,3);
    if(side==0)      pos={(float)GetRandomValue(0,MAP_W),0};
    else if(side==1) pos={(float)MAP_W,(float)GetRandomValue(0,MAP_H)};
    else if(side==2) pos={(float)GetRandomValue(0,MAP_W),(float)MAP_H};
    else             pos={0,(float)GetRandomValue(0,MAP_H)};
    float hp=ZombieHP(g.day);
    g.zombies.push_back({pos,hp,hp,ZombieSpeed(g.day),ZombieDmg(g.day),g.day/10,0,true,0,0});
}

void SpawnWave(Game &g){ int c=SpawnCount(g.day); for(int i=0;i<c;i++) SpawnZombie(g); }

void InitGame(Game &g)
{
    g.state=PLAYING; g.day=1; g.dayTimer=0; g.score=0; g.spawnTimer=0;
    g.player={(Vector2){MAP_W/2.f,MAP_H/2.f},100,100,3,0,0,0,0};
    g.zombies.clear(); g.bullets.clear(); g.bombs.clear(); g.particles.clear();
    SpawnWave(g);
}

// ─── Load Sprites (safe — fallback nếu file không tồn tại) ───────────────────
Texture2D SafeLoad(const char* path, bool &found)
{
    if(FileExists(path)){ found=true; return LoadTexture(path); }
    found=false;
    // Tạo texture trống 1x1 để tránh crash
    Image img=GenImageColor(1,1,BLANK);
    Texture2D t=LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

SpriteAssets LoadAssets()
{
    SpriteAssets a;
    a.player    = SafeLoad("assets/player.png",    a.hasPlayer);
    a.zombie    = SafeLoad("assets/zombie.png",    a.hasZombie);
    a.bullet    = SafeLoad("assets/bullet.png",    a.hasBullet);
    a.bomb      = SafeLoad("assets/bomb.png",      a.hasBomb);
    a.ground    = SafeLoad("assets/ground.png",    a.hasGround);
    a.explosion = SafeLoad("assets/explosion.png", a.hasExplosion);

    // Bật filter đẹp hơn cho các texture
    if(a.hasPlayer)    SetTextureFilter(a.player,    TEXTURE_FILTER_BILINEAR);
    if(a.hasZombie)    SetTextureFilter(a.zombie,    TEXTURE_FILTER_BILINEAR);
    if(a.hasBullet)    SetTextureFilter(a.bullet,    TEXTURE_FILTER_BILINEAR);
    if(a.hasBomb)      SetTextureFilter(a.bomb,      TEXTURE_FILTER_BILINEAR);
    if(a.hasGround)    SetTextureFilter(a.ground,    TEXTURE_FILTER_BILINEAR);
    if(a.hasExplosion) SetTextureFilter(a.explosion, TEXTURE_FILTER_BILINEAR);
    return a;
}

void UnloadAssets(SpriteAssets &a)
{
    UnloadTexture(a.player); UnloadTexture(a.zombie);
    UnloadTexture(a.bullet); UnloadTexture(a.bomb);
    UnloadTexture(a.ground); UnloadTexture(a.explosion);
}

// ─── Draw Sprite helper (vẽ texture căn giữa pos, xoay theo angle) ───────────
void DrawSprite(Texture2D tex, Vector2 pos, float angle, float scale, Color tint)
{
    float w=(float)tex.width*scale;
    float h=(float)tex.height*scale;
    Rectangle src={0,0,(float)tex.width,(float)tex.height};
    Rectangle dst={pos.x,pos.y,w,h};
    Vector2   origin={w/2,h/2};
    DrawTexturePro(tex,src,dst,origin,angle,tint);
}

// Vẽ 1 frame từ spritesheet ngang (frameW = chiều rộng 1 frame)
void DrawSpriteFrame(Texture2D sheet, int frame, int frameW, Vector2 pos, float scale, Color tint)
{
    int cols = sheet.width / frameW;
    if(frame >= cols) frame = cols-1;
    Rectangle src={(float)(frame*frameW),0,(float)frameW,(float)sheet.height};
    float w=(float)frameW*scale, h=(float)sheet.height*scale;
    Rectangle dst={pos.x,pos.y,w,h};
    Vector2 origin={w/2,h/2};
    DrawTexturePro(sheet,src,dst,origin,0,tint);
}

// ─── Health bar ──────────────────────────────────────────────────────────────
void DrawHPBar(Vector2 pos, float hp, float maxHp, float width, float yOff)
{
    float pct=hp/maxHp;
    Color fill=(pct>0.5f)?GREEN:(pct>0.25f?ORANGE:RED);
    DrawRectangle((int)(pos.x-width/2),(int)(pos.y+yOff),(int)width,5,DARKGRAY);
    DrawRectangle((int)(pos.x-width/2),(int)(pos.y+yOff),(int)(width*pct),5,fill);
}

// ─── Update ──────────────────────────────────────────────────────────────────
void Update(Game &g, float dt)
{
    Player &p=g.player;
    float spd=230.f*dt;
    Vector2 moveDir={0,0};
    if(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))    { p.pos.y-=spd; moveDir.y=-1; }
    if(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))  { p.pos.y+=spd; moveDir.y= 1; }
    if(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT))  { p.pos.x-=spd; moveDir.x=-1; }
    if(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)) { p.pos.x+=spd; moveDir.x= 1; }
    p.pos.x = clamp(p.pos.x, 12.0f, (float)MAP_W - 12.0f);
    p.pos.y = clamp(p.pos.y, 12.0f, (float)MAP_H - 12.0f);

    if(VLen(moveDir)>0) p.walkAnim+=dt*8.f;

    if(p.shootCooldown>0) p.shootCooldown-=dt;
    if(p.invincible>0)    p.invincible-=dt;

    Camera2D cam={0};
    cam.target=p.pos; cam.offset={(float)SCREEN_W/2,(float)SCREEN_H/2}; cam.zoom=1.f;

    // Hướng nhìn theo chuột
    Vector2 mw=GetScreenToWorld2D(GetMousePosition(),cam);
    p.angle=atan2f(mw.y-p.pos.y, mw.x-p.pos.x)*RAD2DEG + 90.f;

    // Bắn
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&p.shootCooldown<=0)
    {
        Vector2 dir=VNorm({mw.x-p.pos.x,mw.y-p.pos.y});
        float ang=atan2f(dir.y,dir.x)*RAD2DEG;
        g.bullets.push_back({p.pos,{dir.x*580,dir.y*580},2.2f,true,ang});
        p.shootCooldown=0.12f;
    }

    // Ném bom
    if(IsKeyPressed(KEY_Q)&&p.bombs>0)
    {
        g.bombs.push_back({p.pos,10,0.05f,false,false,0,0});
        p.bombs--;
    }

    // Bullets
    for(auto &b:g.bullets){
        if(!b.active) continue;
        b.pos.x+=b.vel.x*dt; b.pos.y+=b.vel.y*dt; b.life-=dt;
        if(b.life<=0) b.active=false;
    }

    // Bullet vs Zombie
    for(auto &b:g.bullets){
        if(!b.active) continue;
        for(auto &z:g.zombies){
            if(!z.alive) continue;
            float dx=b.pos.x-z.pos.x, dy=b.pos.y-z.pos.y;
            if(sqrtf(dx*dx+dy*dy)<14){
                z.hp-=35; z.flashTimer=0.12f; b.active=false;
                AddParticles(g,z.pos,RED,4,80,3);
                if(z.hp<=0){ z.alive=false; g.score+=10+g.day; AddParticles(g,z.pos,YELLOW,10,140,4); }
                break;
            }
        }
    }

    // Bombs
    for(auto &bomb:g.bombs){
        if(bomb.done) continue;
        bomb.timer-=dt; bomb.radius+=dt*200;
        bomb.frameTimer+=dt;
        if(bomb.frameTimer>0.05f){ bomb.frame++; bomb.frameTimer=0; }
        if(bomb.timer<=0&&!bomb.exploded){
            bomb.exploded=true;
            AddParticles(g,bomb.pos,ORANGE,25,200,6);
            for(auto &z:g.zombies){
                if(!z.alive) continue;
                float dx=bomb.pos.x-z.pos.x, dy=bomb.pos.y-z.pos.y;
                if(sqrtf(dx*dx+dy*dy)<bomb.radius){
                    z.hp-=180; z.flashTimer=0.3f;
                    if(z.hp<=0){ z.alive=false; g.score+=10+g.day; }
                }
            }
        }
        if(bomb.exploded&&bomb.radius>=180) bomb.done=true;
    }
    g.bombs.erase(std::remove_if(g.bombs.begin(),g.bombs.end(),[](const Bomb&b){return b.done;}),g.bombs.end());

    // Zombie AI
    for(auto &z:g.zombies){
        if(!z.alive) continue;
        if(z.flashTimer>0) z.flashTimer-=dt;
        Vector2 dir=VNorm({p.pos.x-z.pos.x,p.pos.y-z.pos.y});
        z.pos.x+=dir.x*z.speed*dt; z.pos.y+=dir.y*z.speed*dt;
        z.angle=atan2f(dir.y,dir.x)*RAD2DEG+90.f;
        z.walkAnim+=dt*6;
        float dx=p.pos.x-z.pos.x, dy=p.pos.y-z.pos.y;
        if(sqrtf(dx*dx+dy*dy)<20&&p.invincible<=0){ p.hp-=z.damage; p.invincible=0.08f; }
    }

    // Particles
    for(auto &pt:g.particles){ pt.pos.x+=pt.vel.x*dt; pt.pos.y+=pt.vel.y*dt; pt.vel.x*=0.92f; pt.vel.y*=0.92f; pt.life-=dt*2; }
    g.particles.erase(std::remove_if(g.particles.begin(),g.particles.end(),[](const Particle&pt){return pt.life<=0;}),g.particles.end());

    // Auto spawn
    g.spawnTimer+=dt;
    int alive=(int)std::count_if(g.zombies.begin(),g.zombies.end(),[](const Zombie&z){return z.alive;});
    if(g.spawnTimer>3.5f&&alive<SpawnCount(g.day)){ SpawnZombie(g); g.spawnTimer=0; }

    // Day cycle
    g.dayTimer+=dt;
    if(g.dayTimer>=DAY_DUR){
        g.dayTimer=0; g.day++; p.bombs=std::min(p.bombs+1,3);
        g.zombies.erase(std::remove_if(g.zombies.begin(),g.zombies.end(),[](const Zombie&z){return !z.alive;}),g.zombies.end());
        SpawnWave(g);
    }

    if(p.hp<=0){ p.hp=0; g.state=GAMEOVER; g.finalDay=g.day; }
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void DrawGame(const Game &g, const SpriteAssets &a)
{
    const Player &p=g.player;
    Camera2D cam={0};
    cam.target=p.pos; cam.offset={(float)SCREEN_W/2,(float)SCREEN_H/2}; cam.zoom=1.f;

    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode2D(cam);

    // ── Ground ───────────────────────────────────────────────────────────────
    if(a.hasGround)
    {
        int tw=a.ground.width, th=a.ground.height;
        for(int y=0;y<MAP_H;y+=th)
            for(int x=0;x<MAP_W;x+=tw)
                DrawTexture(a.ground,x,y,WHITE);
    }
    else
    {
        DrawRectangle(0,0,MAP_W,MAP_H,{42,74,30,255});
        for(int x=0;x<MAP_W;x+=80) DrawLine(x,0,x,MAP_H,{30,55,20,80});
        for(int y=0;y<MAP_H;y+=80) DrawLine(0,y,MAP_W,y,{30,55,20,80});
    }
    DrawRectangleLines(0,0,MAP_W,MAP_H,{200,50,50,200});

    // ── Particles ────────────────────────────────────────────────────────────
    for(const auto &pt:g.particles){
        Color c=pt.color; c.a=(unsigned char)(255*pt.life);
        DrawCircleV(pt.pos,pt.size*pt.life,c);
    }

    // ── Bombs ────────────────────────────────────────────────────────────────
    for(const auto &bomb:g.bombs){
        if(!bomb.exploded){
            if(a.hasBomb) DrawSprite(a.bomb,bomb.pos,0,0.5f,WHITE);
            else { DrawCircleV(bomb.pos,9,ORANGE); DrawCircleLines((int)bomb.pos.x,(int)bomb.pos.y,9,RED); }
        } else {
            if(a.hasExplosion){
                int fw=a.explosion.width/EXPLOSION_FRAMES;
                int frame=std::min(bomb.frame,EXPLOSION_FRAMES-1);
                float sc=(bomb.radius/90.f)*1.5f;
                DrawSpriteFrame(a.explosion,frame,fw,bomb.pos,sc,WHITE);
            } else {
                DrawCircle((int)bomb.pos.x,(int)bomb.pos.y,(int)bomb.radius,{253,186,116,80});
                DrawCircleLines((int)bomb.pos.x,(int)bomb.pos.y,(int)bomb.radius,ORANGE);
            }
        }
    }

    // ── Bullets ──────────────────────────────────────────────────────────────
    for(const auto &b:g.bullets){
        if(!b.active) continue;
        if(a.hasBullet) DrawSprite(a.bullet,b.pos,b.angle+90,0.5f,WHITE);
        else DrawCircleV(b.pos,5,YELLOW);
    }

    // ── Zombies ──────────────────────────────────────────────────────────────
    for(const auto &z:g.zombies){
        if(!z.alive) continue;
        Color tint=z.flashTimer>0?(Color){255,100,100,255}:WHITE;

        // Nhún lên xuống khi đi
        float bob=sinf(z.walkAnim)*2.f;
        Vector2 drawPos={z.pos.x, z.pos.y+bob};

        if(a.hasZombie) DrawSprite(a.zombie,drawPos,z.angle,0.5f,tint);
        else {
            Color body=z.flashTimer>0?(Color){255,160,160,255}:RED;
            DrawCircleV(z.pos,13,body);
            DrawCircle((int)(z.pos.x-4),(int)(z.pos.y-4),3,MAROON);
            DrawCircle((int)(z.pos.x+4),(int)(z.pos.y-4),3,MAROON);
        }

        DrawHPBar(z.pos,z.hp,z.maxHp,26,-22);
        if(z.tier>0){
            std::string t="T"+std::to_string(z.tier+1);
            DrawText(t.c_str(),(int)(z.pos.x-6),(int)(z.pos.y-30),9,YELLOW);
        }
    }

    // ── Player ───────────────────────────────────────────────────────────────
    float bob=sinf(p.walkAnim)*2.f;
    Vector2 pDraw={p.pos.x,p.pos.y+bob};
    if(a.hasPlayer) DrawSprite(a.player,pDraw,p.angle,0.55f,WHITE);
    else { DrawCircleV(p.pos,13,BLUE); DrawCircleV(p.pos,6,SKYBLUE); }
    DrawHPBar(p.pos,p.hp,p.maxHp,30,-22);

    EndMode2D();

    // ── HUD ──────────────────────────────────────────────────────────────────
    DrawText("HP",20,20,20,LIGHTGRAY);
    DrawRectangle(55,20,200,18,DARKGRAY);
    float hpPct=p.hp/p.maxHp;
    Color hpCol=(hpPct>0.5f)?GREEN:(hpPct>0.25f?ORANGE:RED);
    DrawRectangle(55,20,(int)(200*hpPct),18,hpCol);
    DrawRectangleLines(55,20,200,18,GRAY);
    DrawText(TextFormat("%.0f / %.0f",p.hp,p.maxHp),60,22,14,WHITE);

    DrawText(TextFormat("Day %d",g.day),SCREEN_W-180,20,28,YELLOW);
    int tl=(int)(DAY_DUR-g.dayTimer)+1;
    DrawText(TextFormat("Next day: %ds",tl),SCREEN_W/2-80,12,20,LIME);
    DrawText(TextFormat("Score: %d",g.score),SCREEN_W-180,56,20,WHITE);
    DrawText(TextFormat("Bombs: %d",g.player.bombs),SCREEN_W-180,84,20,SKYBLUE);

    int alive=(int)std::count_if(g.zombies.begin(),g.zombies.end(),[](const Zombie&z){return z.alive;});
    DrawText(TextFormat("Zombies: %d",alive),SCREEN_W-180,112,18,{250,100,100,255});

    int tier=g.day/10;
    if(tier>0) DrawText(TextFormat("Tier %d zombies!",tier+1),SCREEN_W/2-80,40,18,ORANGE);

    // Hiển thị trạng thái sprite (debug nhỏ ở góc)
    DrawText(a.hasPlayer?"[player.png OK]":"[player.png missing]",20,SCREEN_H-70,12,a.hasPlayer?LIME:GRAY);
    DrawText(a.hasZombie?"[zombie.png OK]":"[zombie.png missing]",20,SCREEN_H-54,12,a.hasZombie?LIME:GRAY);
    DrawText(a.hasGround?"[ground.png OK]":"[ground.png missing]",20,SCREEN_H-38,12,a.hasGround?LIME:GRAY);
    DrawText("WASD=Move  LMB=Shoot  Q=Bomb",20,SCREEN_H-18,14,DARKGRAY);

    EndDrawing();
}

void DrawMenu(bool isOver, int finalDay, int score)
{
    BeginDrawing();
    ClearBackground({10,10,8,255});
    if(isOver){
        DrawText("YOU DIED",SCREEN_W/2-120,SCREEN_H/2-120,60,RED);
        DrawText(TextFormat("Survived until Day %d",finalDay),SCREEN_W/2-160,SCREEN_H/2-40,28,WHITE);
        DrawText(TextFormat("Final Score: %d",score),SCREEN_W/2-100,SCREEN_H/2,28,YELLOW);
    } else {
        DrawText("ZOMBIE SURVIVAL",SCREEN_W/2-220,SCREEN_H/2-140,56,{220,30,30,255});
        DrawText("Survive as long as possible.",SCREEN_W/2-190,SCREEN_H/2-60,24,LIGHTGRAY);
        DrawText("Every 10 days zombies grow stronger.",SCREEN_W/2-230,SCREEN_H/2-28,22,ORANGE);
    }
    DrawText("WASD / Arrows  -  Move",       SCREEN_W/2-150,SCREEN_H/2+60,20,GRAY);
    DrawText("Left Click     -  Shoot",       SCREEN_W/2-150,SCREEN_H/2+88,20,GRAY);
    DrawText("Q              -  Throw Bomb",  SCREEN_W/2-150,SCREEN_H/2+116,20,GRAY);
    if((int)(GetTime()*2)%2==0)
        DrawText("Press ENTER to start",SCREEN_W/2-160,SCREEN_H/2+170,28,GREEN);
    EndDrawing();
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main()
{
    InitWindow(SCREEN_W,SCREEN_H,"Zombie Survival");
    SetTargetFPS(60);

    SpriteAssets assets=LoadAssets();

    Game g; g.state=MENU; g.finalDay=1; g.score=0;

    while(!WindowShouldClose())
    {
        float dt=GetFrameTime();
        switch(g.state)
        {
        case MENU:
        case GAMEOVER:
            DrawMenu(g.state==GAMEOVER,g.finalDay,g.score);
            if(IsKeyPressed(KEY_ENTER)) InitGame(g);
            break;
        case PLAYING:
            Update(g,dt);
            DrawGame(g,assets);
            break;
        }
    }

    UnloadAssets(assets);
    CloseWindow();
    return 0;
}