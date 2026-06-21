#pragma once
#include <cstdint>
#include <cmath>

// ncurses forward declaration（匹配 ncurses 的 typedef struct _win_st WINDOW）
struct _win_st;
typedef struct _win_st WINDOW;

enum Tile : uint8_t {
    T_EMPTY = 0,
    T_GROUND,
    T_BRICK,
    T_QUESTION,
    T_USED,
    T_HARD,
    T_PIPE_TL, T_PIPE_TR,
    T_PIPE_ML, T_PIPE_MR,
    T_PIPE_BL, T_PIPE_BR,
    T_FLAGPOLE,
    T_FLAGTOP,
    T_COIN_BLOCK,
    T_CASTLE_DOOR,
    T_CASTLE_WIN,
    T_CASTLE_TOP,
    T_INVISIBLE,
    T_TILE_COUNT  // 标记枚举大小，用于查表
};

enum ItemType : uint8_t {
    IT_NONE = 0,
    IT_COIN,
    IT_MUSHROOM,
    IT_FIRE_FLOWER,
    IT_STAR,
    IT_ONEUP,
};

enum EnemyKind : uint8_t {
    EK_GOOMBA = 0,
    EK_KOOPA_GREEN,
    EK_KOOPA_RED,
};

enum PowerState : uint8_t {
    PS_SMALL = 0,
    PS_BIG,
    PS_FIRE,
};

enum GameState : uint8_t {
    GS_TITLE,
    GS_LEVEL_INTRO,
    GS_PLAYING,
    GS_DYING,
    GS_GAME_OVER,
    GS_WIN,
    GS_PAUSE,
};

enum Dir : int8_t {
    D_LEFT = -1,
    D_RIGHT = 1,
};

// ---- Structures ----

struct Entity {
    float x, y;
    float vx, vy;
    Dir dir;
    bool alive;
    bool active;
    int timer;
    EnemyKind kind;
    bool isShell;
    bool shellMoving;
    bool onGround;
    bool squishTimer;
};

struct Fireball {
    float x, y;
    float vx, vy;
    bool alive;
    int life;
};

struct PopText {
    float x, y;
    int life;
    int score;
};

struct CoinEffect {
    float x, y;
    float vy;
    int life;
};

struct Player {
    float x, y;
    float vx, vy;
    Dir dir;
    PowerState power;
    bool onGround;
    bool jumpHeld;
    bool running;
    bool ducking;
    int frame;
    int frameTimer;
    int invTimer;
    int starTimer;
    bool dead;
    bool dying;
    int deathTimer;
    float deathVy;
    bool win;
    bool climbingFlag;
    bool growing;
    int growTimer;
    bool shrinking;
    int shrinkTimer;
    int coins;
    int lives;
    int score;
};

struct EnemySpawn {
    int x, y;
    EnemyKind kind;
};

struct Rect {
    float x, y, w, h;
};

struct BlockItem {
    int x, y;
    ItemType item;
};

struct LevelData {
    int width;
    Tile tiles[15][224];
    int enemyCount;
    EnemySpawn enemies[32];
    int itemCount;
    BlockItem items[64];
    float playerStartX, playerStartY;
    int flagX;
    int castleX;
};

struct Game {
    GameState state;
    Player player;
    LevelData level;
    int cameraX;
    Entity enemies[32];
    int enemyCount;
    Fireball fireballs[4];
    PopText popTexts[16];
    CoinEffect coinEffects[16];
    int timer;
    int frameCount;
    bool inputLeft, inputRight;
    bool inputJump, inputJumpPrev;
    bool inputRun, inputDown;
    bool inputFire, inputFirePrev;
    int worldNum, levelNum;
    bool quit;
    WINDOW* win;        // 类型安全的窗口指针
};

// ---- Constants ----

constexpr int SCREEN_W = 40;
constexpr int SCREEN_H = 15;
constexpr int HUD_ROWS = 2;
constexpr int FPS = 60;
constexpr int FRAME_MS = 1000 / FPS;
constexpr int LEVEL_H = 15;
constexpr float GRAVITY = 0.38f;
constexpr float GRAVITY_LOW = 0.18f;
constexpr float MAX_FALL = 5.5f;
constexpr float WALK_ACCEL = 0.012f;
constexpr float RUN_ACCEL = 0.018f;
constexpr float WALK_MAX = 0.12f;
constexpr float RUN_MAX = 0.28f;
constexpr float SKID_DEC = 0.025f;
constexpr float COAST_DEC = 0.010f;
constexpr float JUMP_VEL_S = -6.2f;
constexpr float JUMP_VEL_B = -7.0f;
// 时间限制已取消
// constexpr int TIME_LIMIT = 600;
constexpr int GOOMBA_SCORE = 100;
constexpr int COIN_SCORE = 200;
constexpr float SHELL_SPEED = 0.35f;
constexpr int INV_DUR = 120;
constexpr int FIREBALL_LIFE = 90;

// ---- Lookup Tables ----

// isSolid 编译期查表 — O(1) 替代链式 ||
constexpr bool SOLID_LOOKUP[T_TILE_COUNT] = {
    [T_EMPTY]     = false,
    [T_GROUND]    = true,  [T_BRICK]   = true,
    [T_QUESTION]  = true,  [T_USED]    = true,
    [T_HARD]      = true,
    [T_PIPE_TL]   = true,  [T_PIPE_TR] = true,
    [T_PIPE_ML]   = true,  [T_PIPE_MR] = true,
    [T_PIPE_BL]   = true,  [T_PIPE_BR] = true,
    [T_FLAGPOLE]  = false, [T_FLAGTOP] = false,
    [T_COIN_BLOCK]= false,
    [T_CASTLE_DOOR]=false, [T_CASTLE_WIN]=false,
    [T_CASTLE_TOP]= false, [T_INVISIBLE]=true,
};

// 瓦片渲染信息：字符 + 颜色对 + 背景色
struct TileRenderInfo {
    int ch;
    int fg;
    int bg;
    int attr;
};

// NES-authentic color palette (closer to actual NES PPU colors)
constexpr int C_RED    = 1;   // NES red
constexpr int C_GRN    = 2;   // NES green
constexpr int C_YEL    = 3;   // NES yellow
constexpr int C_WHT    = 4;   // NES white
constexpr int C_MAG    = 5;   // NES magenta
constexpr int C_CYN    = 6;   // NES cyan/sky
constexpr int C_BLK    = 7;   // Black
constexpr int C_BRN    = 8;   // NES brown/orange
constexpr int C_ORG    = 9;   // NES dark orange
constexpr int C_LGRN   = 10;  // Light green (pipe highlight)
constexpr int C_DGRN   = 11;  // Dark green (pipe shadow)
constexpr int C_TAN    = 12;  // Skin/tan color

// Background color IDs
constexpr int BG_BLK  = 0;
constexpr int BG_BRN   = 13;
constexpr int BG_GRN   = 14;
constexpr int BG_BLU   = 15;
constexpr int BG_YEL   = 16;
constexpr int BG_RED   = 17;
constexpr int BG_GRY   = 18;

// Tile render lookup (for simple tiles)
constexpr TileRenderInfo TILE_RENDER_TABLE[T_TILE_COUNT] = {
    [T_EMPTY]       = {' ',  C_WHT, BG_BLU, 0},
    [T_GROUND]      = {' ',  C_BRN, BG_BRN, 1},
    [T_BRICK]       = {' ',  C_RED, BG_BRN, 1},
    [T_QUESTION]    = {' ',  C_BLK, BG_YEL, 1},
    [T_USED]        = {' ',  C_BRN, BG_BRN, 0},
    [T_HARD]        = {' ',  C_WHT, BG_GRY, 1},
    [T_PIPE_TL]     = {' ',  C_GRN, BG_GRN, 1},
    [T_PIPE_TR]     = {' ',  C_GRN, BG_GRN, 1},
    [T_PIPE_ML]     = {' ',  C_GRN, BG_GRN, 1},
    [T_PIPE_MR]     = {' ',  C_GRN, BG_GRN, 1},
    [T_PIPE_BL]     = {' ',  C_GRN, BG_GRN, 1},
    [T_PIPE_BR]     = {' ',  C_GRN, BG_GRN, 1},
    [T_FLAGPOLE]    = {' ',  C_WHT, BG_BLK, 1},
    [T_FLAGTOP]     = {' ',  C_RED, BG_BLK, 1},
    [T_COIN_BLOCK]  = {0x20BF, C_YEL, BG_BLK, 1},
    [T_CASTLE_DOOR] = {' ',  C_BLK, BG_BRN, 1},
    [T_CASTLE_WIN]  = {' ',  C_WHT, BG_GRY, 1},
    [T_CASTLE_TOP]  = {' ',  C_WHT, BG_GRY, 1},
    [T_INVISIBLE]   = {' ',  C_WHT, BG_BLU, 0},
};

// ---- Inline Helpers ----

inline bool isSolid(Tile t) {
    return t < T_TILE_COUNT && SOLID_LOOKUP[t];
}

inline Tile getTile(const LevelData& lv, int tx, int ty) {
    if (tx < 0 || tx >= lv.width || ty < 0 || ty >= LEVEL_H)
        return T_EMPTY;
    return lv.tiles[ty][tx];
}

inline void setTile(LevelData& lv, int tx, int ty, Tile t) {
    if (tx >= 0 && tx < lv.width && ty >= 0 && ty < LEVEL_H)
        lv.tiles[ty][tx] = t;
}

inline bool rectsOverlap(float ax, float ay, float aw, float ah,
                         float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
