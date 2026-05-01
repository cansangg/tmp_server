#include <raylib.h>
#include <vector>
#include <array>
#include <random>
#include <algorithm>


int main() {
    constexpr int win_h = 1200, win_w = 900;
    InitWindow(win_h, win_w, "C++ Raylib game");
    SetTargetFPS(60);
    
    constexpr int time_interval = 20, H = 16, W = 10;
    std::vector<std::vector<int>> g(H, std::vector<int>(W, 0));
    std::vector<std::array<std::pair<int, int>, 4>> blocks = {
        {{{0, 0}, {1, 0}, {-1, 0}, {-2, 0}}}, //I
        {{{0, 0}, {1, 0}, {0, 1}, {0, -1}}}, //T
        {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}}, //O
        {{{0, 0}, {0, 1}, {1, 0}, {1, -1}}}, //Z
        {{{0, 0}, {0, -1}, {1, 0}, {1, 1}}}, //rZ
        {{{0, 0}, {1, 0}, {-1, 0}, {-1, 1}}}, //L
        {{{0, 0}, {1, 0}, {-1, 0}, {-1, -1}}}, //rL
    };
    std::mt19937 rnd(time(0));

    int current_block, next_block;;
    int cord_x, cord_y, state; // % 4
    int time_cnt, score;
    bool gameover;

    auto onrange = [&](int x, int y) -> bool {
        return x >= 0 && x < H && y >= 0 && y < W;
    };

    auto initgame = [&]() -> void {
        g = std::vector<std::vector<int>>(H, std::vector<int>(W, 0));
        gameover = false;
        time_cnt = score = 0;
        current_block = rnd() % blocks.size(), next_block = rnd() % blocks.size();
        cord_x = H - 2, cord_y = W / 2, state = 0;
    };

    auto rotate = [&](int& x, int& y, int sta) -> void {
        while (sta--) std::tie(x, y) = std::tuple(-y, x);
    };

    auto try_move = [&](int dx, int dy) -> bool {
        cord_x += dx, cord_y += dy;
        bool ok = true;
        for (auto [x, y] : blocks[current_block]) {
            rotate(x, y, state);
            x += cord_x, y += cord_y;
            if (!onrange(x, y) || g[x][y]) ok = false;
        }
        if (!ok) cord_x -= dx, cord_y -= dy;
        return ok;
    };

    auto try_rotate = [&]() -> bool {
        state = (state + 1) % 4;
        bool ok = true;
        for (auto [x, y] : blocks[current_block]) {
            rotate(x, y, state);
            x += cord_x, y += cord_y;
            if (!onrange(x, y) || g[x][y]) ok = false;
        }
        if (!ok) state = (state - 1 + 4) % 4;
        return ok;
    };

    auto place_and_loadnext_and_checkgameover = [&]() -> void {
        for (auto [x, y] : blocks[current_block]) {
            rotate(x, y, state);
            x += cord_x, y += cord_y;
            g[x][y] = 1;
        }

        std::vector<std::vector<int>> ng;
        for (auto& v : g) {
            if (std::accumulate(v.begin(), v.end(), 0) < W) {
                ng.push_back(std::move(v));
            } else {
                score += 100;
            }
        }
        while (ng.size() < H) ng.push_back(std::vector<int>(W));
        swap(g, ng);

        current_block = next_block, next_block = rnd() % blocks.size();
        cord_x = H - 2, cord_y = W / 2, state = 0;

        if (!try_move(0, 0)) gameover = true;
    };

    auto render = [&]() -> void {
        // ==================================================
        // render (纯正笛卡尔坐标系: x行 y列，原点左下角)
        // ==================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // 动态计算渲染尺寸，让画面在 1200x900 的大窗口里完美居中
        const int CELL_SIZE = 50; // 格子放大到 50 像素，视觉效果极佳
        // 注意：你传给 InitWindow 的第一个参数是 win_h(1200)，所以它是宽
        const int OFFSET_X = (win_h - W * CELL_SIZE) / 2; 
        const int OFFSET_Y = (win_w - H * CELL_SIZE) / 2; 

        // 1. 画游戏区域底板和边框
        DrawRectangle(OFFSET_X, OFFSET_Y, W * CELL_SIZE, H * CELL_SIZE, LIGHTGRAY);
        // 加粗外围边框
        DrawRectangleLines(OFFSET_X - 2, OFFSET_Y - 2, W * CELL_SIZE + 4, H * CELL_SIZE + 4, BLACK); 

        // 2. 画已经固定的方块 (g 数组)
        for (int i = 0; i < H; ++i) {     // i 是行 (0为最底下)
            for (int j = 0; j < W; ++j) { // j 是列 (0为最左边)
                if (g[i][j] != 0) {
                    int render_x = OFFSET_X + j * CELL_SIZE;
                    // 【核心翻转映射】：屏幕Y = 偏移 + (总高 H - 1 - 当前行 i) * 尺寸
                    int render_y = OFFSET_Y + (H - 1 - i) * CELL_SIZE; 
                    // CELL_SIZE - 2 留下 2 像素的空隙，质感拉满
                    DrawRectangle(render_x, render_y, CELL_SIZE - 2, CELL_SIZE - 2, GRAY);
                }
            }
        }

        // 3. 画当前正在下落的方块
        if (!gameover) {
            for (auto [x, y] : blocks[current_block]) {
                int cx = x, cy = y;
                rotate(cx, cy, state);
                cx += cord_x; 
                cy += cord_y;

                if (cx >= 0 && cx < H && cy >= 0 && cy < W) {
                    int render_x = OFFSET_X + cy * CELL_SIZE;
                    int render_y = OFFSET_Y + (H - 1 - cx) * CELL_SIZE;
                    DrawRectangle(render_x, render_y, CELL_SIZE - 2, CELL_SIZE - 2, DARKBLUE);
                }
            }
        }

        // 4. 画右侧的下一个方块预览区
        int preview_x = OFFSET_X + W * CELL_SIZE + 50;
        int preview_y = OFFSET_Y + 50;
        DrawText("NEXT:", preview_x, preview_y, 30, DARKGRAY);
        
        for (auto [x, y] : blocks[next_block]) {
            // y控制左右，x控制上下(这里也按左下角法则渲染)
            int render_x = preview_x + (y + 2) * CELL_SIZE;
            int render_y = preview_y + 80 + (2 - x) * CELL_SIZE; 
            DrawRectangle(render_x, render_y, CELL_SIZE - 2, CELL_SIZE - 2, SKYBLUE);
        }

        // 5. 计分板和状态 UI
        DrawText(TextFormat("SCORE: %d", score), 50, OFFSET_Y, 40, DARKGRAY);

        if (gameover) {
            // 盖一层全屏的半透明黑色遮罩，高级感直接拉满
            DrawRectangle(0, 0, win_h, win_w, Fade(BLACK, 0.6f));
            
            // MeasureText 会自动计算字符串在特定字号下的宽度，保证居中极其完美
            const char* go_text = "GAME OVER!";
            const char* re_text = "Press [ENTER] to Restart";
            DrawText(go_text, win_h / 2 - MeasureText(go_text, 80) / 2, win_w / 2 - 60, 80, RED);
            DrawText(re_text, win_h / 2 - MeasureText(re_text, 30) / 2, win_w / 2 + 40, 30, LIGHTGRAY);
        }

        EndDrawing();
    };


    initgame();
    while (!WindowShouldClose()) {
        // handle input and update
        if (!gameover) {
            if (IsKeyPressed(KEY_UP)) try_rotate();
            if (IsKeyPressed(KEY_LEFT)) try_move(0, -1);
            if (IsKeyPressed(KEY_RIGHT)) try_move(0, 1);
            if (IsKeyPressed(KEY_DOWN)) try_move(-1, 0);
            if (++time_cnt > time_interval) {
                time_cnt = 0;
                if (!try_move(-1, 0)) place_and_loadnext_and_checkgameover();
            }
        } else {
            if (IsKeyPressed(KEY_ENTER)) initgame();
        }

        //render
        render();
    }

    CloseWindow();
    return 0;
}