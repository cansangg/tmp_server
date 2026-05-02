#include <bits/stdc++.h>

int main() {
    std::function<void(int)> func;
    {
        int y = 666;
        auto get = [&y](int x) -> void {
            std::cout << x + y << '\n';
        };
        func = get;
    }
    {
        volatile int z = 333;
        volatile int w = 333;
        volatile int v = 333;
    }
    func(333);

    return 0;
}