#pragma once
#include "defs.h"

struct RNG {
    unsigned int seed;
    RNG(unsigned int s) : seed(s) {}
    int next() {
        seed = seed * 1103515245 + 12345;
        return (seed >> 16) & 0x7FFF;
    }
    int range(int lo, int hi) { return lo + next() % (hi - lo + 1); }
};

inline void buildLevel(LevelData& lv, int world, int level) {
    lv.width = 210 + (world - 1) * 3;
    if (lv.width > 240) lv.width = 240;
    lv.playerStartX = 3.0f;
    lv.playerStartY = 11.0f;
    lv.enemyCount = 0;
    lv.itemCount = 0;

    int totalLevel = (world - 1) * 4 + level;
    int theme = ((world - 1) / 5) % 4;
    RNG rng(totalLevel * 7919 + 31337);

    for (int r = 0; r < 15; r++)
        for (int c = 0; c < lv.width; c++)
            lv.tiles[r][c] = T_EMPTY;

    bool isUnderground = (theme == 1);
    bool isSky = (theme == 2);
    bool isCastle = (theme == 3);

    auto ground = [&](int x) {
        if (!isSky) {
            lv.tiles[13][x] = isCastle ? T_HARD : T_GROUND;
            lv.tiles[14][x] = isCastle ? T_HARD : T_GROUND;
        }
    };
    for (int c = 0; c < lv.width; c++) ground(c);

    if (isUnderground || isCastle) {
        for (int c = 0; c < lv.width; c++) lv.tiles[0][c] = T_HARD;
    }

    auto brick = [&](int x, int y) { lv.tiles[y][x] = T_BRICK; };
    auto qblock = [&](int x, int y, ItemType it) {
        if (x >= 0 && x < lv.width && y >= 0 && y < 15) {
            lv.tiles[y][x] = T_QUESTION;
            if (lv.itemCount < 64)
                lv.items[lv.itemCount++] = {x, y, it};
        }
    };
    auto flagpole = [&](int x) {
        for (int r = 1; r <= 12; r++) lv.tiles[r][x] = T_FLAGPOLE;
        lv.tiles[0][x] = T_FLAGTOP;
    };
    auto pipe = [&](int x, int h) {
        if (x < 0 || x + 1 >= lv.width) return;
        int topRow = 13 - h;
        if (topRow < 1) topRow = 1;
        lv.tiles[topRow][x]   = T_PIPE_TL;
        lv.tiles[topRow][x+1] = T_PIPE_TR;
        for (int r = topRow+1; r < 13; r++) {
            lv.tiles[r][x]   = T_PIPE_ML;
            lv.tiles[r][x+1] = T_PIPE_MR;
        }
    };
    auto pit = [&](int x, int w) {
        for (int i = 0; i < w; i++) {
            if (x+i < lv.width) {
                lv.tiles[13][x+i] = T_EMPTY;
                lv.tiles[14][x+i] = T_EMPTY;
            }
        }
    };
    auto stair = [&](int x, int h, bool asc) {
        for (int i = 0; i < h; i++) {
            int col = asc ? x + i : x + h - 1 - i;
            for (int r = 13 - i - 1; r < 13; r++) {
                if (col >= 0 && col < lv.width)
                    lv.tiles[r][col] = isCastle ? T_HARD : T_GROUND;
            }
        }
    };
    auto platform = [&](int x, int y, int w) {
        for (int i = 0; i < w; i++) {
            if (x+i >= 0 && x+i < lv.width && y >= 0 && y < 15)
                lv.tiles[y][x+i] = T_HARD;
        }
    };
    auto enemy_add = [&](int x, int y, EnemyKind k) {
        if (lv.enemyCount < 32 && x >= 0 && x < lv.width)
            lv.enemies[lv.enemyCount++] = {x, y, k};
    };

    int diff = (world - 1) * 4 + (level - 1);
    int numPits = 3 + diff / 4;
    int numPipes = 3 + diff / 5;
    int numEnemies = 8 + diff / 2;
    int numBricks = 6 + diff / 3;
    int numQBlocks = 3 + diff / 6;
    int numStairs = 1 + diff / 8;
    int numPlatforms = isSky ? 6 + diff / 3 : 2 + diff / 6;

    if (numPits > 12) numPits = 12;
    if (numEnemies > 28) numEnemies = 28;
    if (numPipes > 10) numPipes = 10;

    int segmentWidth = (lv.width - 20) / (numPits + 1);
    if (segmentWidth < 8) segmentWidth = 8;

    int placedX = 10;
    for (int i = 0; i < numPits && placedX < lv.width - 20; i++) {
        int pitW = rng.range(2, 3 + diff / 15);
        if (pitW > 5) pitW = 5;
        pit(placedX, pitW);
        placedX += pitW + rng.range(12, 20);
    }

    for (int i = 0; i < numPipes && placedX < lv.width - 15; i++) {
        int px = rng.range(15, lv.width - 20);
        int ph = rng.range(2, 3 + diff / 20);
        if (ph > 5) ph = 5;
        pipe(px, ph);
    }

    for (int i = 0; i < numBricks; i++) {
        int bx = rng.range(10, lv.width - 20);
        int by = rng.range(5, 9);
        brick(bx, by);
        if (rng.next() % 3 == 0 && bx + 1 < lv.width)
            brick(bx + 1, by);
        if (rng.next() % 4 == 0 && by >= 3)
            brick(bx, by - 4);
    }

    for (int i = 0; i < numQBlocks; i++) {
        int qx = rng.range(12, lv.width - 15);
        int qy = rng.range(5, 9);
        ItemType it = IT_COIN;
        if (rng.next() % 4 == 0) it = IT_MUSHROOM;
        else if (rng.next() % 6 == 0) it = IT_STAR;
        qblock(qx, qy, it);
    }

    for (int i = 0; i < numStairs && placedX < lv.width - 15; i++) {
        int sx = rng.range(placedX, placedX + 8);
        int sh = rng.range(3, 5 + diff / 20);
        if (sh > 8) sh = 8;
        stair(sx, sh, true);
        stair(sx + sh + rng.range(2, 5), sh, false);
        placedX = sx + sh * 2 + 10;
    }

    for (int i = 0; i < numPlatforms; i++) {
        int px = rng.range(15, lv.width - 20);
        int py = rng.range(4, 8);
        int pw = rng.range(3, 6);
        platform(px, py, pw);
        if (rng.next() % 3 == 0)
            qblock(px + pw / 2, py - 1, IT_COIN);
    }

    if (isSky) {
        for (int c = 0; c < lv.width; c++) {
            lv.tiles[13][c] = T_EMPTY;
            lv.tiles[14][c] = T_EMPTY;
        }
        for (int i = 0; i < lv.width / 15; i++) {
            int bx = i * 15 + rng.range(0, 5);
            for (int j = 0; j < rng.range(4, 8); j++) {
                if (bx + j < lv.width) {
                    lv.tiles[13][bx+j] = T_HARD;
                    lv.tiles[14][bx+j] = T_HARD;
                }
            }
        }
        for (int i = 0; i < 8 + diff / 3; i++) {
            int px = rng.range(5, lv.width - 10);
            int py = rng.range(5, 11);
            int pw = rng.range(2, 4);
            platform(px, py, pw);
        }
    }

    lv.flagX = lv.width - 12;
    lv.castleX = lv.width - 7;

    flagpole(lv.flagX);

    int cx = lv.castleX;
    for (int c2 = cx; c2 < cx+5; c2++) {
        lv.tiles[13][c2] = T_HARD;
        lv.tiles[12][c2] = T_HARD;
    }
    for (int r = 7; r <= 11; r++) {
        lv.tiles[r][cx] = T_HARD;
        lv.tiles[r][cx+4] = T_HARD;
    }
    for (int c2 = cx+1; c2 < cx+4; c2++) {
        lv.tiles[7][c2] = T_HARD;
        lv.tiles[6][c2] = T_HARD;
    }
    lv.tiles[10][cx+2] = T_CASTLE_DOOR;
    lv.tiles[11][cx+2] = T_CASTLE_DOOR;
    lv.tiles[9][cx+1] = T_CASTLE_WIN;
    lv.tiles[9][cx+3] = T_CASTLE_WIN;

    for (int i = 0; i < numEnemies; i++) {
        int ex = rng.range(15, lv.width - 20);
        int ey = 12;
        EnemyKind ek = EK_GOOMBA;
        if (rng.next() % 4 == 0) ek = EK_KOOPA_GREEN;
        else if (rng.next() % 6 == 0) ek = EK_KOOPA_RED;
        bool onPlatform = false;
        for (int r = 0; r < 13; r++) {
            if (isSolid(getTile(lv, ex, r))) {
                ey = r - 1;
                onPlatform = true;
                break;
            }
        }
        if (!onPlatform) ey = 12;
        enemy_add(ex, ey, ek);
    }
}
