#include "defs.h"
#include "game.h"
#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ======================================================================
//  RAII 封装 — ncurses 窗口生命周期
// ======================================================================

struct NcursesGuard {
    WINDOW* win;
    NcursesGuard() {
        win = initscr();
        cbreak();
        noecho();
        nodelay(stdscr, TRUE);
        curs_set(0);
        start_color();
        use_default_colors();

        // NES-authentic color palette
        init_pair(C_RED,  COLOR_RED,    COLOR_BLACK);  // Mario red, brick red
        init_pair(C_GRN,  COLOR_GREEN,  COLOR_BLACK);  // Pipe green, bush green
        init_pair(C_YEL,  COLOR_YELLOW, COLOR_BLACK);  // Question block yellow, coin
        init_pair(C_WHT,  COLOR_WHITE,  COLOR_BLACK);  // Cloud, text
        init_pair(C_MAG,  COLOR_MAGENTA,COLOR_BLACK);  // Fire flower
        init_pair(C_CYN,  COLOR_CYAN,   COLOR_BLACK);  // Sky highlight
        init_pair(C_BLK,  COLOR_WHITE,  COLOR_BLACK);  // Goomba eyes
        init_pair(C_BRN,  COLOR_YELLOW, COLOR_BLACK);  // Goomba brown
        init_pair(C_ORG,  COLOR_RED,    COLOR_BLACK);  // Brick orange

        // Background color pairs for tiles
        init_pair(BG_BRN, COLOR_YELLOW, 40);    // Ground brown (dark orange bg)
        init_pair(BG_GRN, COLOR_GREEN,  COLOR_GREEN);  // Pipe green bg
        init_pair(BG_BLU, COLOR_BLUE,   COLOR_BLUE);   // Sky blue bg
        init_pair(BG_YEL, COLOR_BLACK,  COLOR_YELLOW); // Question block yellow bg
        init_pair(BG_RED, COLOR_RED,    COLOR_RED);    // Red bg
        init_pair(BG_GRY, COLOR_WHITE,  COLOR_WHITE);  // Gray bg

        bkgd(COLOR_PAIR(BG_BLU));  // Classic NES sky blue background
        erase();
        refresh();
    }
    ~NcursesGuard() {
        endwin();
    }
    NcursesGuard(const NcursesGuard&) = delete;
    NcursesGuard& operator=(const NcursesGuard&) = delete;
};

// ======================================================================
//  帧计时 — 基于 clock_nanosleep 的精确帧同步
// ======================================================================

struct FrameTimer {
    struct timespec next;

    void init() {
        clock_gettime(CLOCK_MONOTONIC, &next);
    }

    // 等待至下一帧，返回当前帧的 timespec（可用于调试）
    void wait() {
        next.tv_nsec += FRAME_MS * 1000000L;
        // 处理纳秒溢出
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
    }
};

// ======================================================================
//  输入处理
// ======================================================================

static void processInput(Game& g) {
    // 保存跳/射击的前一帧状态（边沿检测用）
    g.inputJumpPrev = g.inputJump;
    g.inputFirePrev = g.inputFire;

    // 清空每帧输入（按键状态由 getch 实时决定）
    g.inputLeft   = false;
    g.inputRight  = false;
    g.inputDown   = false;
    g.inputRun    = false;

    int ch;
    while ((ch = getch()) != ERR) {
        switch (ch) {
            case 'a': case 'A': case KEY_LEFT:   g.inputLeft = true;  break;
            case 'd': case 'D': case KEY_RIGHT:  g.inputRight = true; break;
            case 's': case 'S': case KEY_DOWN:   g.inputDown = true;  break;
            case 'w': case 'W': case KEY_UP:
            case 'z': case 'Z': case ' ':        g.inputJump = true;  break;
            case 'x': case 'X':
                g.inputRun = true;
                g.inputFire = true;
                break;
            case 'q': case 'Q':   g.quit = true; break;
            case 'p': case 'P':
                if (g.state == GS_PLAYING) g.state = GS_PAUSE;
                else if (g.state == GS_PAUSE) g.state = GS_PLAYING;
                break;
        }
    }
}

// ======================================================================
//  游戏状态机
// ======================================================================

static void runGameState(Game& g, int& introTimer) {
    switch (g.state) {
        case GS_TITLE:
            renderTitle(g);
            if (g.inputJump && !g.inputJumpPrev) {
                loadLevel(g);
                g.state = GS_LEVEL_INTRO;
                introTimer = 120;
            }
            break;

        case GS_LEVEL_INTRO:
            renderLevelIntro(g);
            introTimer--;
            if (introTimer <= 0) g.state = GS_PLAYING;
            break;

        case GS_PLAYING:
        case GS_DYING:
            updateGame(g);
            renderGame(g);
            break;

        case GS_PAUSE: {
            WINDOW* w = g.win;
            wattron(w, COLOR_PAIR(C_YEL));
            mvwprintw(w, 7, 15, "PAUSED");
            wattroff(w, COLOR_PAIR(C_YEL));
            wrefresh(w);
            break;
        }

        case GS_GAME_OVER:
            renderGameOver(g);
            // 让玩家看到 GAME OVER 画面 2 秒
            {
                FrameTimer ft;
                ft.init();
                ft.wait();
                ft.wait();  // 约 33ms * 60 = ~2s
                // 实际用简单等待
                for (int i = 0; i < 120; i++) ft.wait();
            }
            memset(&g.player, 0, sizeof(Player));
            g.player.lives = 3;
            g.player.coins = 0;
            g.player.score = 0;
            g.state = GS_TITLE;
            break;

        case GS_WIN:
            renderWin(g);
            if (g.inputJump && !g.inputJumpPrev) {
                 g.player.score += g.timer * 50;
                g.levelNum++;
                if (g.levelNum > 4) {
                    g.levelNum = 1;
                    g.worldNum++;
                }
                if (g.worldNum > 20) {
                    g.state = GS_TITLE;
                    g.worldNum = 1;
                    g.levelNum = 1;
                    g.player.lives = 3;
                    g.player.score = 0;
                    g.player.coins = 0;
                } else {
                    loadLevel(g);
                    g.state = GS_LEVEL_INTRO;
                    introTimer = 120;
                }
            }
            break;
    }
}

// ======================================================================
//  主入口
// ======================================================================

int main() {
    NcursesGuard nc;  // RAII：初始化 ncurses，退出时自动 endwin()

    Game game;
    memset(&game, 0, sizeof(Game));
    game.state = GS_TITLE;
    game.win = nc.win;          // 类型安全的窗口指针
    game.worldNum = 1;
    game.levelNum = 1;
    game.player.lives = 3;
    game.player.coins = 0;
    game.player.score = 0;
    game.quit = false;

    FrameTimer ft;
    ft.init();

    int introTimer = 120;

    while (!game.quit) {
        processInput(game);
        runGameState(game, introTimer);
        ft.wait();   // 精确 60 FPS 同步
    }

    return 0;
}
