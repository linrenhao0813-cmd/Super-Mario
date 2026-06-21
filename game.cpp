#include "defs.h"
#include "level_data.h"
#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ======================================================================
//  工具函数
// ======================================================================

static inline int getPlayerHeight(const Player& p) {
    return (p.power != PS_SMALL && !p.ducking) ? 2 : 1;
}

// ======================================================================
//  关卡加载
// ======================================================================

void loadLevel(Game& g) {
    buildLevel(g.level, g.worldNum, g.levelNum);
    Player& p = g.player;
    p.x = g.level.playerStartX;
    p.y = g.level.playerStartY;
    p.vx = 0; p.vy = 0;
    p.dir = D_RIGHT;
    p.onGround = false;
    p.dead = false;
    p.dying = false;
    p.win = false;
    p.climbingFlag = false;
    p.ducking = false;
    p.growing = false;
    p.shrinking = false;
    p.invTimer = 0;
    p.starTimer = 0;
    p.frame = 0;
    p.frameTimer = 0;
    // 时间限制已取消
    // g.timer = TIME_LIMIT;
    g.frameCount = 0;
    g.cameraX = 0;
    g.enemyCount = g.level.enemyCount;

    for (int i = 0; i < g.enemyCount; i++) {
        const EnemySpawn& es = g.level.enemies[i];
        g.enemies[i] = {(float)es.x, (float)es.y, -0.05f, 0, D_LEFT, true, false, 0, es.kind,
                         false, false, false, false};
    }
    for (int i = 0; i < 4; i++) g.fireballs[i] = {0,0,0,0,false,0};
    for (int i = 0; i < 16; i++) {
        g.popTexts[i] = {0,0,0,0};
        g.coinEffects[i] = {0,0,0,0};
    }
}

// ======================================================================
//  玩家 update — 拆分为多个小函数
// ======================================================================

/// 处理左右移动输入及加速度/减速度
static void handlePlayerMovement(Game& g) {
    Player& p = g.player;
    float maxSpd = p.running ? RUN_MAX : WALK_MAX;
    float acc   = p.running ? RUN_ACCEL : WALK_ACCEL;

    if (g.inputRight) {
        if (p.vx < 0 && p.onGround) {
            p.vx += SKID_DEC;
            if (p.vx > 0) p.vx = 0;
        } else {
            p.vx += acc;
            if (p.vx > maxSpd) p.vx = maxSpd;
        }
        p.dir = D_RIGHT;
    } else if (g.inputLeft) {
        if (p.vx > 0 && p.onGround) {
            p.vx -= SKID_DEC;
            if (p.vx < 0) p.vx = 0;
        } else {
            p.vx -= acc;
            if (p.vx < -maxSpd) p.vx = -maxSpd;
        }
        p.dir = D_LEFT;
    } else if (p.onGround) {
        if (p.vx > 0) {
            p.vx -= COAST_DEC;
            if (p.vx < 0) p.vx = 0;
        } else if (p.vx < 0) {
            p.vx += COAST_DEC;
            if (p.vx > 0) p.vx = 0;
        }
    }
}

/// 处理跳跃、重力
static void handlePlayerJump(Game& g) {
    Player& p = g.player;
    if (g.inputJump && !p.jumpHeld && p.onGround) {
        p.vy = (p.power == PS_SMALL) ? JUMP_VEL_S : JUMP_VEL_B;
        if (p.running) p.vy -= 0.3f;
        p.onGround = false;
        p.jumpHeld = true;
    }
    if (!g.inputJump) p.jumpHeld = false;

    p.vy += g.inputJump ? GRAVITY_LOW : GRAVITY;
    if (p.vy > MAX_FALL) p.vy = MAX_FALL;
}

/// 水平碰撞检测
static void handleHorizontalCollision(Game& g, float newX, int mh, float mw, float mxo) {
    Player& p = g.player;
    bool blocked = false;
    for (int pass = 0; pass < 2; pass++) {
        float cx = (pass == 0) ? newX + mxo : newX + mw + mxo;
        int tx = (int)cx;
        int ty1 = (int)(p.y + 0.1f);
        int ty2 = (int)(p.y + mh - 0.1f);
        for (int ty = ty1; ty <= ty2; ty++) {
            if (isSolid(getTile(g.level, tx, ty))) { blocked = true; break; }
        }
        if (blocked) break;
    }
    if (blocked) {
        if (p.vx > 0)
            p.x = (int)(newX + mxo + mw) - mw - mxo - 0.001f;
        else
            p.x = (int)(newX + mxo) + 1.0f - mxo + 0.001f;
        p.vx = 0;
    } else {
        p.x = newX;
    }
}

/// 垂直碰撞检测（下落/顶头）+ 方块交互
static void handleVerticalCollision(Game& g, float& newY, int mh, float mw, float mxo) {
    Player& p = g.player;
    bool groundHit = false;

    // 下落检测
    if (p.vy >= 0) {
        float footY = newY + mh;
        int ty = (int)footY;
        int tx1 = (int)(p.x + mxo + 0.1f);
        int tx2 = (int)(p.x + mw + mxo - 0.1f);
        for (int tx = tx1; tx <= tx2; tx++) {
            if (isSolid(getTile(g.level, tx, ty))) {
                groundHit = true;
                newY = (float)ty - mh;
                p.vy = 0;
                break;
            }
        }
    }

    // 头顶碰撞 + 问号砖/砖块交互
    if (p.vy < 0) {
        float headY = newY;
        int ty = (int)headY;
        int tx1 = (int)(p.x + mxo + 0.1f);
        int tx2 = (int)(p.x + mw + mxo - 0.1f);
        for (int tx = tx1; tx <= tx2; tx++) {
            Tile t = getTile(g.level, tx, ty);
            if (isSolid(t)) {
                newY = (float)(ty + 1);
                p.vy = 0.5f;

                if (t == T_QUESTION) {
                    ItemType it = IT_COIN;
                    for (int i = 0; i < g.level.itemCount; i++) {
                        if (g.level.items[i].x == tx && g.level.items[i].y == ty) {
                            it = g.level.items[i].item; break;
                        }
                    }
                    setTile(g.level, tx, ty, T_USED);
                    if (it == IT_COIN) {
                        p.coins++; p.score += COIN_SCORE;
                        if (p.coins >= 100) { p.coins -= 100; p.lives++; }
                        for (int i = 0; i < 16; i++) {
                            if (g.coinEffects[i].life <= 0) {
                                g.coinEffects[i] = {(float)tx, (float)ty - 1, -0.3f, 30};
                                break;
                            }
                        }
                    } else if (it == IT_MUSHROOM || it == IT_STAR) {
                        for (int i = 0; i < g.enemyCount; i++) {
                            if (!g.enemies[i].alive && !g.enemies[i].active) {
                                g.enemies[i] = {(float)tx, (float)ty - 1, 0.08f, 0,
                                    D_RIGHT, true, true, 0, EK_GOOMBA,
                                    it == IT_MUSHROOM, false, false, false};
                                break;
                            }
                        }
                    }
                } else if (t == T_BRICK) {
                    if (p.power != PS_SMALL) {
                        setTile(g.level, tx, ty, T_EMPTY);
                        p.score += 50;
                    }
                }
                break;
            }
        }
    }

    if (p.vy >= 0) {
        p.onGround = groundHit;
    }
    p.y = newY;
}

/// 处理死亡 / 到达旗杆
static void handlePlayerDeathAndWin(Game& g) {
    Player& p = g.player;
    if (p.y > LEVEL_H + 2) {
        p.dying = true; p.deathTimer = 0; p.deathVy = -3;
        g.state = GS_DYING;
        return;
    }
    // 左边界
    if (p.x < g.cameraX + 0.5f) {
        p.x = g.cameraX + 0.5f;
        if (p.vx < 0) p.vx = 0;
    }
    // 旗杆
    if (!p.climbingFlag && !p.win && (int)p.x >= g.level.flagX - 1) {
        p.climbingFlag = true; p.vx = 0; p.vy = 0;
        p.x = (float)(g.level.flagX - 1) + 0.5f;
        p.win = true;
    }
}

/// 更新玩家动画帧
static void updatePlayerAnimation(Player& p) {
    p.frameTimer++;
    int spd = p.running ? 4 : 6;
    if (p.frameTimer >= spd) {
        p.frameTimer = 0;
        p.frame = (p.frame + 1) % 3;
    }
}

/// 更新玩家（主入口）
static void updatePlayer(Game& g) {
    Player& p = g.player;

    // —— 死亡动画 ——
    if (p.dying) {
        p.deathTimer++;
        if (p.deathTimer > 10) {
            p.deathVy += 0.15f;
            p.y += p.deathVy * 0.1f;
        }
        if (p.deathTimer > 180) {
            p.lives--;
            if (p.lives <= 0) g.state = GS_GAME_OVER;
            else g.state = GS_LEVEL_INTRO;
        }
        return;
    }

    // —— 胜利动画 ——
    if (p.win) {
        if (p.climbingFlag) {
            p.y += 0.15f;
            if (p.y >= 12.0f) { p.y = 12.0f; p.climbingFlag = false; }
        } else {
            p.x += 0.1f;
            if (p.x > g.level.castleX + 2) g.state = GS_WIN;
        }
        return;
    }

    // —— 变大/变小动画 ——
    if (p.growing) { p.growTimer++; if (p.growTimer > 50) p.growing = false; return; }
    if (p.shrinking) { p.shrinkTimer++; if (p.shrinkTimer > 50) { p.shrinking = false; p.invTimer = INV_DUR; } return; }

    // 计时器
    if (p.invTimer > 0) p.invTimer--;
    if (p.starTimer > 0) p.starTimer--;

    int mh = getPlayerHeight(p);
    constexpr float mw = 0.8f;
    constexpr float mxo = 0.1f;

    handlePlayerMovement(g);
    handlePlayerJump(g);

    float newX = p.x + p.vx;
    handleHorizontalCollision(g, newX, mh, mw, mxo);

    float newY = p.y + p.vy * 0.05f;
    handleVerticalCollision(g, newY, mh, mw, mxo);

    handlePlayerDeathAndWin(g);
    updatePlayerAnimation(p);
}

// ======================================================================
//  敌人 update
// ======================================================================

static void updateEnemies(Game& g) {
    for (int i = 0; i < g.enemyCount; i++) {
        Entity& e = g.enemies[i];
        if (!e.alive) continue;
        if (!e.active) {
            if (e.x - g.player.x < SCREEN_W + 2) e.active = true;
            continue;
        }
        if (e.squishTimer) { e.timer++; if (e.timer > 30) e.alive = false; continue; }

        if (!e.isShell) {
            e.vy += GRAVITY;
            if (e.vy > MAX_FALL) e.vy = MAX_FALL;
        }

        float newX = e.x + e.vx;
        int tx = (e.vx > 0) ? (int)(newX + 0.9f) : (int)newX;
        int ty1 = (int)(e.y + 0.1f);
        int ty2 = (int)(e.y + 0.9f);
        bool wall = false;
        for (int ty = ty1; ty <= ty2; ty++) {
            if (isSolid(getTile(g.level, tx, ty))) { wall = true; break; }
        }
        if (wall) { e.vx = -e.vx; e.dir = e.vx > 0 ? D_RIGHT : D_LEFT; }
        else e.x = newX;

        float newY = e.y + e.vy * 0.05f;
        float footY = newY + 1.0f;
        int fty = (int)footY;
        int ftx1 = (int)(e.x + 0.1f);
        int ftx2 = (int)(e.x + 0.9f);
        for (int tx2 = ftx1; tx2 <= ftx2; tx2++) {
            if (isSolid(getTile(g.level, tx2, fty))) {
                newY = (float)fty - 1.0f;
                e.vy = 0;
                e.onGround = true;
                break;
            }
        }
        e.y = newY;
        if (e.y > LEVEL_H + 2) e.alive = false;
    }
}

// ======================================================================
//  碰撞检测（玩家 vs 敌人）
// ======================================================================

/// 弹出分数文字
static void spawnPopText(PopText texts[16], float x, float y, int score) {
    for (int i = 0; i < 16; i++) {
        if (texts[i].life <= 0) {
            texts[i] = {x, y - 0.5f, 30, score};
            return;
        }
    }
}

static void checkEnemyCollisions(Game& g) {
    Player& p = g.player;
    if (p.dying || p.win || p.growing || p.shrinking) return;

    int mh = getPlayerHeight(p);
    Rect pr = {p.x + 0.1f, p.y + 0.1f, 0.8f, (float)mh - 0.2f};

    for (int i = 0; i < g.enemyCount; i++) {
        Entity& e = g.enemies[i];
        if (!e.alive || !e.active || e.squishTimer) continue;

        float eh = e.isShell ? 0.7f : 0.9f;
        float eyo = e.isShell ? 0.3f : 0.1f;
        Rect er = {e.x, e.y + eyo, 0.9f, eh};
        if (!rectsOverlap(pr.x, pr.y, pr.w, pr.h, er.x, er.y, er.w, er.h)) continue;

        // 无敌星
        if (p.starTimer > 0) {
            e.alive = false; p.score += GOOMBA_SCORE;
            spawnPopText(g.popTexts, e.x, e.y, GOOMBA_SCORE);
            continue;
        }

        // 踩踏
        if (p.vy > 0 && pr.y + pr.h < er.y + er.h * 0.5f) {
            p.vy = -3.0f; p.onGround = false;
            if (e.isShell) {
                if (e.shellMoving) { e.shellMoving = false; e.vx = 0; }
                else { e.shellMoving = true; e.vx = (p.x < e.x) ? SHELL_SPEED : -SHELL_SPEED; }
            } else if (e.kind == EK_KOOPA_GREEN || e.kind == EK_KOOPA_RED) {
                e.isShell = true; e.shellMoving = false; e.vx = 0; e.y += 0.2f;
            } else {
                e.squishTimer = true; e.timer = 0; e.vy = 0;
            }
            p.score += GOOMBA_SCORE;
            spawnPopText(g.popTexts, e.x, e.y, GOOMBA_SCORE);
        } else {
            // 被敌人碰到
            if (e.isShell && !e.shellMoving) {
                e.shellMoving = true;
                e.vx = (p.x < e.x) ? SHELL_SPEED : -SHELL_SPEED;
            } else if (!e.isShell || e.shellMoving) {
                if (p.invTimer <= 0) {
                    if (p.power == PS_SMALL) {
                        p.dying = true; p.deathTimer = 0; p.deathVy = -3;
                        g.state = GS_DYING;
                    } else {
                        p.power = PS_SMALL;
                        p.shrinking = true; p.shrinkTimer = 0;
                    }
                }
            }
        }
    }
}

// ======================================================================
//  火球 update
// ======================================================================

static void updateFireballs(Game& g) {
    Player& p = g.player;
    if (g.inputFire && !g.inputFirePrev && p.power == PS_FIRE) {
        for (int i = 0; i < 4; i++) {
            if (!g.fireballs[i].alive) {
                g.fireballs[i] = {p.x + (p.dir == D_RIGHT ? 0.8f : -0.2f), p.y + 0.5f,
                                   (float)p.dir * 0.3f, 0, true, FIREBALL_LIFE};
                break;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        Fireball& fb = g.fireballs[i];
        if (!fb.alive) continue;
        fb.life--;
        if (fb.life <= 0) { fb.alive = false; continue; }
        fb.x += fb.vx;
        fb.vy += GRAVITY * 0.5f;
        fb.y += fb.vy * 0.05f;
        int tx = (int)fb.x, ty = (int)fb.y;
        if (isSolid(getTile(g.level, tx, ty))) { fb.alive = false; continue; }
        int fty = (int)(fb.y + 0.5f);
        int ftx = (int)(fb.x + 0.3f);
        if (isSolid(getTile(g.level, ftx, fty))) { fb.vy = -2.0f; fb.y = (float)fty - 0.6f; }
        for (int j = 0; j < g.enemyCount; j++) {
            Entity& e = g.enemies[j];
            if (!e.alive || !e.active || e.squishTimer) continue;
            if (rectsOverlap(fb.x, fb.y, 0.3f, 0.3f, e.x, e.y, 0.9f, 0.9f)) {
                e.alive = false; fb.alive = false; p.score += GOOMBA_SCORE;
                spawnPopText(g.popTexts, e.x, e.y, GOOMBA_SCORE);
                break;
            }
        }
    }
}

// ======================================================================
//  弹出文字 / 金币特效 update
// ======================================================================

static void updatePopTexts(Game& g) {
    for (int i = 0; i < 16; i++) {
        if (g.popTexts[i].life > 0) { g.popTexts[i].y -= 0.05f; g.popTexts[i].life--; }
        if (g.coinEffects[i].life > 0) {
            g.coinEffects[i].y += g.coinEffects[i].vy;
            g.coinEffects[i].vy += 0.02f;
            g.coinEffects[i].life--;
        }
    }
}

// ======================================================================
//  Game Update 主入口
// ======================================================================

void updateGame(Game& g) {
    if (g.state != GS_PLAYING) return;
    g.frameCount++;
    // 时间限制已取消
    // if (g.frameCount % FPS == 0 && g.timer > 0) {
    //     g.timer--;
    //     if (g.timer <= 0) {
    //         g.player.dying = true; g.player.deathTimer = 0; g.player.deathVy = -3;
    //         g.state = GS_DYING;
    //     }
    // }
    updatePlayer(g);
    updateEnemies(g);
    checkEnemyCollisions(g);
    updateFireballs(g);
    updatePopTexts(g);

    float tx = g.player.x - SCREEN_W / 2.0f;
    if (tx < 0) tx = 0;
    if (tx > g.level.width - SCREEN_W) tx = (float)(g.level.width - SCREEN_W);
    g.cameraX = (int)tx;
}

// ======================================================================
//  渲染 — 瓦片（批量切换颜色对）
// ======================================================================

/// Helper: draw a single half-block pixel cell using fg/bg colors
/// top=true: draw upper half block (▀), top=fg, bottom=bg
/// top=false: draw lower half block (▄), top=bg, bottom=fg
static inline void drawHalfBlock(WINDOW* w, int row, int col, int fg, int /*bg*/, bool top) {
    int ch = top ? 0x2580 : 0x2584; // ▀ or ▄
    wattron(w, COLOR_PAIR(fg));
    mvwaddch(w, row, col, ch | COLOR_PAIR(fg));
    wattroff(w, COLOR_PAIR(fg));
}

/// Render tile using pixel-art half-block sprites
static void renderTiles(Game& g, WINDOW* w, int camX) {
    for (int sy = 0; sy < SCREEN_H; sy++) {
        for (int sx = 0; sx < SCREEN_W; sx++) {
            int tx = sx + camX;
            if (tx < 0 || tx >= g.level.width) continue;
            Tile t = g.level.tiles[sy][tx];
            if (t == T_EMPTY || t >= T_TILE_COUNT) continue;

            int row = sy + HUD_ROWS;

            switch (t) {
                case T_GROUND: {
                    // NES ground: brown block
                    wattron(w, COLOR_PAIR(C_BRN) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2588 | A_BOLD); // █ full block
                    wattroff(w, COLOR_PAIR(C_BRN) | A_BOLD);
                    break;
                }
                case T_BRICK: {
                    // NES brick: red/orange pattern
                    int ch = ((sx + sy) % 2 == 0) ? 0x2592 : 0x2591; // ▒ or ░
                    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
                    mvwaddch(w, row, sx, ch | A_BOLD);
                    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);
                    break;
                }
                case T_QUESTION: {
                    // NES question block: yellow with animated ?
                    int phase = (g.frameCount / 12) % 4;
                    int ch = '?';
                    if (phase == 1) ch = '!';
                    else if (phase == 2) ch = 0x25A0; // ■
                    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
                    mvwaddch(w, row, sx, ch | A_BOLD);
                    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);
                    break;
                }
                case T_USED: {
                    // Used block: dark brown/gray
                    wattron(w, COLOR_PAIR(C_BRN));
                    mvwaddch(w, row, sx, 0x2591); // ░
                    wattroff(w, COLOR_PAIR(C_BRN));
                    break;
                }
                case T_HARD: {
                    // Hard block: gray stone
                    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2588 | A_BOLD); // █
                    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    break;
                }
                case T_PIPE_TL:
                case T_PIPE_TR: {
                    // Pipe top: green with light green highlight
                    wattron(w, COLOR_PAIR(C_LGRN) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2588 | A_BOLD); // █
                    wattroff(w, COLOR_PAIR(C_LGRN) | A_BOLD);
                    break;
                }
                case T_PIPE_ML:
                case T_PIPE_MR: {
                    // Pipe middle: green body with highlight
                    int ch = (tx % 2 == 0) ? ' ' : 0x2588;
                    int fg = (tx % 2 == 0) ? C_GRN : C_LGRN;
                    wattron(w, COLOR_PAIR(fg) | A_BOLD);
                    mvwaddch(w, row, sx, ch | A_BOLD);
                    wattroff(w, COLOR_PAIR(fg) | A_BOLD);
                    break;
                }
                case T_PIPE_BL:
                case T_PIPE_BR: {
                    // Pipe bottom: green with border
                    wattron(w, COLOR_PAIR(C_GRN) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2588 | A_BOLD); // █
                    wattroff(w, COLOR_PAIR(C_GRN) | A_BOLD);
                    break;
                }
                case T_FLAGPOLE: {
                    // Flagpole: white vertical line
                    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2503); // ┃
                    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    break;
                }
                case T_FLAGTOP: {
                    // Flag: red triangle
                    int ch = ((g.frameCount / 8) % 2 == 0) ? 0x25BC : 0x25B2;
                    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
                    mvwaddch(w, row, sx, ch | A_BOLD);
                    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);
                    break;
                }
                case T_COIN_BLOCK: {
                    // Coin block: yellow coin symbol
                    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
                    mvwaddch(w, row, sx, 0x20BF); // ¢
                    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);
                    break;
                }
                case T_CASTLE_DOOR: {
                    // Castle door: black rectangle
                    wattron(w, COLOR_PAIR(C_BLK));
                    mvwaddch(w, row, sx, 0x2588); // █
                    wattroff(w, COLOR_PAIR(C_BLK));
                    break;
                }
                case T_CASTLE_WIN: {
                    // Castle window: white cross
                    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    mvwaddch(w, row, sx, 0x25A1); // □
                    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    break;
                }
                case T_CASTLE_TOP: {
                    // Castle top: gray crenellation
                    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    mvwaddch(w, row, sx, 0x2580); // ▀
                    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

// ======================================================================
//  渲染 — HUD
// ======================================================================

/// Render HUD matching classic NES Super Mario Bros layout
static void renderHUD(Game& g, WINDOW* w) {
    // Classic NES HUD: MARIO    *00    WORLD  TIME
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 0, 0, "MARIO");
    mvwprintw(w, 0, 6, "%06d", g.player.score);
    mvwprintw(w, 0, 14, "*%02d", g.player.coins);
    mvwprintw(w, 0, 20, "WORLD");
    mvwprintw(w, 0, 26, "%d-%d", g.worldNum, g.levelNum);
    mvwprintw(w, 0, 32, "TIME");
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    // Coin icon
    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
    mvwaddch(w, 0, 13, 0x20BF); // ¢
    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);
}

// ======================================================================
//  渲染 — 玩家
// ======================================================================

/// Helper: draw a pixel-art Mario sprite using half-blocks
/// Small Mario: 2 chars wide x 2 rows (4x4 pixels)
/// Big Mario: 2 chars wide x 3 rows (4x6 pixels)
static void drawMarioSprite(WINDOW* w, int startRow, int startCol, 
                            PowerState power, int /*frame*/, Dir /*dir*/, 
                            bool ducking, bool starTimer, int frameCount) {
    // NES Mario colors
    int hatColor = C_RED;      // Red hat
    int skinColor = C_TAN;     // Skin
    int overallColor = C_RED;  // Red overalls (small mario)
    
    if (power == PS_FIRE) {
        hatColor = C_WHT;      // White hat
        overallColor = C_RED;  // Red overalls
    }
    
    // Star power: cycle colors
    if (starTimer > 0) {
        int cycle = (frameCount / 3) % 4;
        if (cycle == 0) { hatColor = C_RED; overallColor = C_RED; }
        else if (cycle == 1) { hatColor = C_GRN; overallColor = C_GRN; }
        else if (cycle == 2) { hatColor = C_YEL; overallColor = C_YEL; }
        else { hatColor = C_CYN; overallColor = C_CYN; }
    }
    
    int row = startRow;
    int col = startCol;
    
    if (power == PS_SMALL || ducking) {
        // Small Mario: 2 chars wide x 2 rows
        // Row 0: Hat (▀▄ = red top, red bottom)
        wattron(w, COLOR_PAIR(hatColor) | A_BOLD);
        mvwaddch(w, row, col, 0x2580 | A_BOLD);     // ▀ red top
        mvwaddch(w, row, col + 1, 0x2580 | A_BOLD); // ▀ red top
        wattroff(w, COLOR_PAIR(hatColor) | A_BOLD);
        
        // Row 1: Face + body
        wattron(w, COLOR_PAIR(skinColor) | A_BOLD);
        mvwaddch(w, row + 1, col, 0x2584);     // ▄ skin bottom
        wattroff(w, COLOR_PAIR(skinColor) | A_BOLD);
        
        wattron(w, COLOR_PAIR(overallColor) | A_BOLD);
        mvwaddch(w, row + 1, col + 1, 0x2584); // ▄ overall bottom
        wattroff(w, COLOR_PAIR(overallColor) | A_BOLD);
    } else {
        // Big Mario: 2 chars wide x 3 rows
        // Row 0: Hat
        wattron(w, COLOR_PAIR(hatColor) | A_BOLD);
        mvwaddch(w, row, col, 0x2588 | A_BOLD);     // █ full block
        mvwaddch(w, row, col + 1, 0x2588 | A_BOLD); // █ full block
        wattroff(w, COLOR_PAIR(hatColor) | A_BOLD);
        
        // Row 1: Face
        wattron(w, COLOR_PAIR(skinColor) | A_BOLD);
        mvwaddch(w, row + 1, col, 0x2588);     // █ skin
        mvwaddch(w, row + 1, col + 1, 0x2588); // █ skin
        wattroff(w, COLOR_PAIR(skinColor) | A_BOLD);
        
        // Row 2: Overalls
        wattron(w, COLOR_PAIR(overallColor) | A_BOLD);
        mvwaddch(w, row + 2, col, 0x2588);     // █ overall
        mvwaddch(w, row + 2, col + 1, 0x2588); // █ overall
        wattroff(w, COLOR_PAIR(overallColor) | A_BOLD);
    }
}

/// Render player with pixel-art NES Mario sprites
static void renderPlayer(Game& g, WINDOW* w, int camX) {
    Player& p = g.player;
    if (p.dying && p.deathTimer <= 10) return;

    int sx = (int)(p.x - camX);
    int sy = (int)p.y;
    if (sx < -1 || sx >= SCREEN_W + 1 || sy < 0 || sy >= SCREEN_H) return;

    // Invincibility flicker
    if (p.invTimer > 0 && (g.frameCount / 3) % 2 == 0) return;

    int row = sy + HUD_ROWS;
    if (p.ducking && p.power != PS_SMALL) row++;

    // Draw Mario sprite
    drawMarioSprite(w, row, sx, p.power, p.frame, p.dir, 
                    p.ducking, p.starTimer, g.frameCount);
}

// ======================================================================
//  渲染 — 敌人
// ======================================================================

/// Helper: draw Goomba pixel-art sprite (brown mushroom)
static void drawGoombaSprite(WINDOW* w, int row, int col, bool squished) {
    if (squished) {
        // Squished Goomba: flat brown line
        wattron(w, COLOR_PAIR(C_BRN) | A_BOLD);
        mvwaddch(w, row + 1, col, 0x2584); // ▄ brown bottom
        wattroff(w, COLOR_PAIR(C_BRN) | A_BOLD);
        return;
    }
    // Goomba: brown cap top, lighter bottom
    wattron(w, COLOR_PAIR(C_BRN) | A_BOLD);
    mvwaddch(w, row, col, 0x2580);     // ▀ brown top (cap)
    wattroff(w, COLOR_PAIR(C_BRN) | A_BOLD);
    
    wattron(w, COLOR_PAIR(C_ORG) | A_BOLD);
    mvwaddch(w, row + 1, col, 0x2584); // ▄ orange bottom (face)
    wattroff(w, COLOR_PAIR(C_ORG) | A_BOLD);
}

/// Helper: draw Koopa pixel-art sprite (green turtle)
static void drawKoopaSprite(WINDOW* w, int row, int col, bool isShell) {
    if (isShell) {
        // Koopa shell: green oval
        wattron(w, COLOR_PAIR(C_GRN) | A_BOLD);
        mvwaddch(w, row, col, 0x2580);     // ▀ green top
        wattroff(w, COLOR_PAIR(C_GRN) | A_BOLD);
        
        wattron(w, COLOR_PAIR(C_DGRN) | A_BOLD);
        mvwaddch(w, row + 1, col, 0x2584); // ▄ dark green bottom
        wattroff(w, COLOR_PAIR(C_DGRN) | A_BOLD);
        return;
    }
    // Koopa Troopa: green shell with head
    wattron(w, COLOR_PAIR(C_GRN) | A_BOLD);
    mvwaddch(w, row, col, 0x2588);     // █ green top (shell)
    wattroff(w, COLOR_PAIR(C_GRN) | A_BOLD);
    
    wattron(w, COLOR_PAIR(C_LGRN) | A_BOLD);
    mvwaddch(w, row + 1, col, 0x2584); // ▄ light green bottom
    wattroff(w, COLOR_PAIR(C_LGRN) | A_BOLD);
}

/// Render enemies with pixel-art NES sprites
static void renderEnemies(Game& g, WINDOW* w, int camX) {
    for (int i = 0; i < g.enemyCount; i++) {
        const Entity& e = g.enemies[i];
        if (!e.alive || !e.active) continue;
        int sx = (int)(e.x - camX);
        int sy = (int)e.y;
        if (sx < -1 || sx > SCREEN_W + 1 || sy < 0 || sy >= SCREEN_H) continue;
        
        int row = sy + HUD_ROWS;
        
        if (e.squishTimer) {
            drawGoombaSprite(w, row, sx, true);
        } else if (e.isShell) {
            drawKoopaSprite(w, row, sx, true);
        } else if (e.kind == EK_GOOMBA) {
            drawGoombaSprite(w, row, sx, false);
        } else {
            drawKoopaSprite(w, row, sx, false);
        }
    }
}

// ======================================================================
//  渲染 — 火球
// ======================================================================

static void renderFireballs(Game& g, WINDOW* w, int camX) {
    for (int i = 0; i < 4; i++) {
        const Fireball& fb = g.fireballs[i];
        if (!fb.alive) continue;
        int sx = (int)(fb.x - camX);
        int sy = (int)fb.y;
        if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H) {
            wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
            mvwaddch(w, sy + HUD_ROWS, sx, '*' | A_BOLD);
            wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);
        }
    }
}

// ======================================================================
//  渲染 — 弹出文字 & 金币特效
// ======================================================================

static void renderPopTexts(Game& g, WINDOW* w, int camX) {
    for (int i = 0; i < 16; i++) {
        if (g.popTexts[i].life > 0) {
            int sx = (int)(g.popTexts[i].x - camX);
            int sy = (int)g.popTexts[i].y;
            if (sx >= 0 && sx < SCREEN_W - 2 && sy >= 0 && sy < SCREEN_H) {
                wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
                mvwprintw(w, sy + HUD_ROWS, sx, "%d", g.popTexts[i].score);
                wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
            }
        }
        if (g.coinEffects[i].life > 0) {
            int sx = (int)(g.coinEffects[i].x - camX);
            int sy = (int)g.coinEffects[i].y;
            if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H) {
            wattron(w, COLOR_PAIR(C_YEL));
            mvwaddstr(w, sy + HUD_ROWS, sx, "*");
            wattroff(w, COLOR_PAIR(C_YEL));
            }
        }
    }
}

// ======================================================================
//  背景装饰 (云朵、灌木、山丘)
// ======================================================================

/// Render classic NES-style decorations using pixel-art half-blocks
static void renderDecorations(Game& g, WINDOW* w, int camX) {
    int theme = ((g.worldNum - 1) / 5) % 4;
    if (theme == 1 || theme == 3) return;

    // Clouds - white pixel-art clouds
    for (int i = 0; i < g.level.width / 20; i++) {
        int cx = i * 20 + 5;
        int cy = 1 + (i % 3);
        int screenX = cx - camX;
        if (screenX >= -6 && screenX < SCREEN_W + 6) {
            // Top row: white top
            wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
            mvwaddch(w, cy + HUD_ROWS, screenX, 0x2580);     // ▀
            mvwaddch(w, cy + HUD_ROWS, screenX + 1, 0x2580); // ▀
            mvwaddch(w, cy + HUD_ROWS, screenX + 2, 0x2580); // ▀
            wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);
            // Bottom row: cyan sky
            wattron(w, COLOR_PAIR(C_CYN));
            mvwaddch(w, cy + HUD_ROWS + 1, screenX, 0x2584);     // ▄
            mvwaddch(w, cy + HUD_ROWS + 1, screenX + 1, 0x2584); // ▄
            mvwaddch(w, cy + HUD_ROWS + 1, screenX + 2, 0x2584); // ▄
            wattroff(w, COLOR_PAIR(C_CYN));
        }
    }

    // Bushes - green pixel-art bushes
    for (int i = 0; i < g.level.width / 16; i++) {
        int bx = i * 16 + 8;
        int screenX = bx - camX;
        if (screenX >= -5 && screenX < SCREEN_W + 5) {
            // Top row: green top
            wattron(w, COLOR_PAIR(C_GRN) | A_BOLD);
            mvwaddch(w, 12 + HUD_ROWS, screenX, 0x2580);     // ▀
            mvwaddch(w, 12 + HUD_ROWS, screenX + 1, 0x2580); // ▀
            mvwaddch(w, 12 + HUD_ROWS, screenX + 2, 0x2580); // ▀
            wattroff(w, COLOR_PAIR(C_GRN) | A_BOLD);
            // Bottom row: light green bottom
            wattron(w, COLOR_PAIR(C_LGRN));
            mvwaddch(w, 13 + HUD_ROWS, screenX, 0x2584);     // ▄
            mvwaddch(w, 13 + HUD_ROWS, screenX + 1, 0x2584); // ▄
            mvwaddch(w, 13 + HUD_ROWS, screenX + 2, 0x2584); // ▄
            wattroff(w, COLOR_PAIR(C_LGRN));
        }
    }

    // Hills - green pixel-art hills
    for (int i = 0; i < g.level.width / 24; i++) {
        int hx = i * 24 + 2;
        int screenX = hx - camX;
        if (screenX >= -7 && screenX < SCREEN_W + 7) {
            // Hill peak: green triangle
            wattron(w, COLOR_PAIR(C_GRN) | A_BOLD);
            mvwaddch(w, 11 + HUD_ROWS, screenX, 0x2580);     // ▀
            mvwaddch(w, 11 + HUD_ROWS, screenX + 1, 0x2580); // ▀
            mvwaddch(w, 11 + HUD_ROWS, screenX + 2, 0x2580); // ▀
            wattroff(w, COLOR_PAIR(C_GRN) | A_BOLD);
            // Hill base: dark green
            wattron(w, COLOR_PAIR(C_DGRN));
            mvwaddch(w, 12 + HUD_ROWS, screenX, 0x2584);     // ▄
            mvwaddch(w, 12 + HUD_ROWS, screenX + 1, 0x2584); // ▄
            mvwaddch(w, 12 + HUD_ROWS, screenX + 2, 0x2584); // ▄
            wattroff(w, COLOR_PAIR(C_DGRN));
        }
    }
}

// ======================================================================
//  Render 主入口
// ======================================================================

void renderGame(Game& g) {
    WINDOW* w = g.win;
    werase(w);

    renderHUD(g, w);
    renderDecorations(g, w, g.cameraX);
    renderTiles(g, w, g.cameraX);
    renderEnemies(g, w, g.cameraX);
    renderFireballs(g, w, g.cameraX);
    renderPlayer(g, w, g.cameraX);
    renderPopTexts(g, w, g.cameraX);

    wrefresh(w);
}

// ======================================================================
//  标题画面 / 关卡介绍 / Game Over / 胜利画面
// ======================================================================

/// Classic NES Super Mario Bros title screen
void renderTitle(Game& g) {
    WINDOW* w = g.win;
    werase(w);

    // Top border
    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
    for (int x = 0; x < SCREEN_W; x++)
        mvwaddch(w, 0, x, '-' | A_BOLD);
    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);

    // Title - SUPER MARIO BROS.
    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
    mvwprintw(w, 2, 8, "SUPER MARIO BROS.");
    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);

    // Decorative line
    wattron(w, COLOR_PAIR(C_YEL));
    mvwprintw(w, 3, 10, "================");
    wattroff(w, COLOR_PAIR(C_YEL));

    // Mario icon and menu
    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
    mvwaddch(w, 6, 16, '@');  // Mario icon
    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);

    wattron(w, COLOR_PAIR(C_WHT));
    mvwprintw(w, 6, 18, "1 PLAYER GAME");
    wattroff(w, COLOR_PAIR(C_WHT));

    // Menu selector
    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
    mvwaddch(w, 6, 15, '>');
    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);

    // Controls info box
    wattron(w, COLOR_PAIR(C_GRN));
    for (int x = 2; x < 38; x++)
        mvwaddch(w, 8, x, '.');
    mvwprintw(w, 9, 12, "CONTROLS");
    mvwprintw(w, 10, 8, "A-Left  D-Right");
    mvwprintw(w, 11, 8, "W-Jump  S-Duck");
    mvwprintw(w, 12, 8, "X-Run   SPACE-Fire");
    wattroff(w, COLOR_PAIR(C_GRN));

    // Bottom info
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 13, 8, "80 LEVELS - 20 WORLDS");
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    // Bottom border
    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
    for (int x = 0; x < SCREEN_W; x++)
        mvwaddch(w, 14, x, '-' | A_BOLD);
    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);

    wrefresh(w);
}

/// Classic NES level intro screen
void renderLevelIntro(Game& g) {
    WINDOW* w = g.win;
    werase(w);

    int theme = ((g.worldNum - 1) / 5) % 4;
    const char* themeName = "";
    int themeColor = C_WHT;

    switch (theme) {
        case 0: themeName = "GRASS LAND";      themeColor = C_GRN;  break;
        case 1: themeName = "UNDERGROUND";     themeColor = C_WHT; break;
        case 2: themeName = "SKY WORLD";       themeColor = C_YEL; break;
        case 3: themeName = "BOWSER CASTLE";   themeColor = C_RED;  break;
    }

    // World number - centered
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 4, 14, "WORLD %d-%d", g.worldNum, g.levelNum);
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    // Theme name
    wattron(w, COLOR_PAIR(themeColor) | A_BOLD);
    mvwprintw(w, 6, (40 - (int)strlen(themeName)) / 2, "%s", themeName);
    wattroff(w, COLOR_PAIR(themeColor) | A_BOLD);

    // Lives display - Mario icon x lives
    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
    mvwaddstr(w, 9, 10, "@");
    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 9, 12, "x %d", g.player.lives);
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    wrefresh(w);
}

/// Classic NES game over screen
void renderGameOver(Game& g) {
    WINDOW* w = g.win;
    werase(w);

    // GAME OVER text
    wattron(w, COLOR_PAIR(C_RED) | A_BOLD);
    mvwprintw(w, 6, 14, "GAME OVER");
    wattroff(w, COLOR_PAIR(C_RED) | A_BOLD);

    // Score display
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 9, 10, "SCORE: %06d", g.player.score);
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    wrefresh(w);
}

/// Classic NES course clear screen
void renderWin(Game& g) {
    WINDOW* w = g.win;
    werase(w);

    // COURSE CLEAR! text
    wattron(w, COLOR_PAIR(C_YEL) | A_BOLD);
    mvwprintw(w, 3, 11, "COURSE CLEAR!");
    wattroff(w, COLOR_PAIR(C_YEL) | A_BOLD);

    // Star decoration
    wattron(w, COLOR_PAIR(C_YEL));
    mvwaddch(w, 4, 18, '*');
    wattroff(w, COLOR_PAIR(C_YEL));

    // Score display
    wattron(w, COLOR_PAIR(C_WHT) | A_BOLD);
    mvwprintw(w, 6, 10, "SCORE:  %06d", g.player.score);
    mvwprintw(w, 8, 10, "TIME BONUS: %d x 50", g.timer);
    mvwprintw(w, 10, 10, "TOTAL: %06d", g.player.score + g.timer * 50);
    wattroff(w, COLOR_PAIR(C_WHT) | A_BOLD);

    // Press key to continue
    wattron(w, COLOR_PAIR(C_WHT));
    mvwprintw(w, 13, 8, "PRESS W TO CONTINUE");
    wattroff(w, COLOR_PAIR(C_WHT));

    wrefresh(w);
}

