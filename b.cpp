#include <vector>
#include <array>
#include <random>
#include <algorithm>

#include "my_SelectPoller.hpp"


int main() {
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
    int gameover;

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

    initgame();

    my::SelectPoller poller(8080);
    std::cout << "started listening" << '\n';

    auto onNewConnection = [&]() -> void {
        poller.m_clients.push_back(std::move(poller.m_server.acceptClient()));
        std::cout << poller.m_clients.back().getFd() << " enter" << '\n';
    };

    auto onClientData = [&](my::TcpSocket& client) -> void {
        std::string c = client.readExactly(1);
        if (!gameover) {
            if (c == "U") try_rotate(), std::cout << client.getFd() << " pressed U" << '\n';
            if (c == "L") try_move(0, -1), std::cout << client.getFd() << " pressed L" << '\n';
            if (c == "R") try_move(0, 1), std::cout << client.getFd() << " pressed R" << '\n';
            if (c == "D") try_move(-1, 0), std::cout << client.getFd() << " pressed D" << '\n';
            if (++time_cnt > time_interval) {
                time_cnt = 0;
                if (!try_move(-1, 0)) place_and_loadnext_and_checkgameover();
            }
        } else {
            if (c == "E") initgame(), std::cout << client.getFd() << " pressed E" << '\n';
        }
        if (c == "") {
            for (auto it = poller.m_clients.begin(); it != poller.m_clients.end(); ++it) {
                if (client.getFd() == it->getFd()) {
                    std::cout << client.getFd() << " exit" << '\n';
                    poller.m_clients.erase(it);
                    break;
                }
            }
        }
    };

    while (true) {
        poller.poll(1000 / 16, onNewConnection, onClientData);
        for (auto& client : poller.m_clients) {
            for (auto v : g) for (auto x : v) client.write(std::to_string(x) + "\n");
            client.write(std::to_string(cord_x) + "\n");
            client.write(std::to_string(cord_y) + "\n");
            client.write(std::to_string(state) + "\n");
            client.write(std::to_string(current_block) + "\n");
            client.write(std::to_string(next_block) + "\n");
            client.write(std::to_string(score) + "\n");
            client.write(std::to_string(gameover) + "\n");
        }
    }

    return 0;
}