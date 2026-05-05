#include <raylib.h>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <iostream>

#include "my_TcpSocket.hpp"


int main() {
    constexpr int win_w = 1200, win_h = 900;
    InitWindow(win_w, win_h, "C++ Raylib game");
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

    int current_block = 0, next_block = 1;
    int cord_x = H - 2, cord_y = W / 2, state = 0; // % 4
    int time_cnt = 0, score = 0;
    int gameover = 1;

    auto render = [&]() -> void {
        // ==================================================
        // render (纯正笛卡尔坐标系: x行 y列，原点左下角)
        // ==================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // 动态计算渲染尺寸，让画面在 1200x900 的大窗口里完美居中
        const int CELL_SIZE = 50; // 格子放大到 50 像素，视觉效果极佳
        // 注意：你传给 InitWindow 的第一个参数是 win_w(1200)，所以它是宽
        const int OFFSET_X = (win_w - W * CELL_SIZE) / 2; 
        const int OFFSET_Y = (win_h - H * CELL_SIZE) / 2; 

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

                 auto rotate = [&](int& x, int& y, int sta) -> void {
                    while (sta--) std::tie(x, y) = std::tuple(-y, x);
                };

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
            DrawRectangle(0, 0, win_w, win_h, Fade(BLACK, 0.6f));
            
            // MeasureText 会自动计算字符串在特定字号下的宽度，保证居中极其完美
            const char* go_text = "GAME OVER!";
            const char* re_text = "Press [ENTER] to Restart";
            DrawText(go_text, win_w / 2 - MeasureText(go_text, 80) / 2, win_h / 2 - 60, 80, RED);
            DrawText(re_text, win_w / 2 - MeasureText(re_text, 30) / 2, win_h / 2 + 40, 30, LIGHTGRAY);
        }

        EndDrawing();
    };

    my::TcpSocket client;
    client.connectTo("47.238.99.207", 8080);
    std::cout << "connected" << std::endl;

    struct p_class {
        int g[16][10];
        int cord_x, cord_y, state;
        int current_block, next_block;
        int score;
        int gameover;
        int time_cnt;
    };

    int cnt = 0;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_UP))    client.write("U"), std::cout << "pressed U\n";
        if (IsKeyPressed(KEY_LEFT))  client.write("L"), std::cout << "pressed L\n";
        if (IsKeyPressed(KEY_RIGHT)) client.write("R"), std::cout << "pressed R\n";
        if (IsKeyPressed(KEY_DOWN))  client.write("D"), std::cout << "pressed D\n";
        if (IsKeyPressed(KEY_ENTER)) client.write("E"), std::cout << "pressed E\n";

        std::string data;
        while (true) {
            std::optional<std::string> opt_current_data = client.readExactly(sizeof(p_class)); // 非堵塞读取
            if (!opt_current_data || opt_current_data->empty()) break; // 抽干了，跳出
            data = std::move(*opt_current_data); // 永远覆盖，只留最新的
        }
        if (client.isClosed()) break; //服务端关闭连接
        
        if (!data.empty()) { //延迟返回空串时防memcpy报错
            p_class p;
            std::memcpy(&p, data.data(), sizeof(p_class));
            
            for (int i = 0; i < H; ++i) for (int j = 0; j < W; ++j) g[i][j] = p.g[i][j];
            cord_x = p.cord_x; 
            cord_y = p.cord_y; 
            state = p.state;
            current_block = p.current_block; 
            next_block = p.next_block;
            score = p.score; 
            gameover = p.gameover;
            time_cnt = p.time_cnt;
        }
        
        render();
    }

    CloseWindow();
    return 0;
}