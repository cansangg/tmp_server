#include <iostream>
#include <thread>
#include <future>

void make_coffee(std::promise<std::string> p) {
    std::cout << "[后厨] 正在做咖啡..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟耗时
    
    // 【关键动作】咖啡做好了，放到吧台！
    // 这行代码在底层会自动唤醒拿着小票等候的人
    p.set_value("拿铁咖啡"); 
}

int main() {
    // 1. 创建订单（生成一套管道）
    std::promise<std::string> order_promise;
    
    // 2. 拿到这单的小票
    std::future<std::string> receipt = order_promise.get_future();

    // 3. 把后厨的工作交给一个新线程去干，记得把 promise 传进去（必须用 std::move，因为承诺只能给一次）
    std::thread t(make_coffee, std::move(order_promise));

    std::cout << "[顾客] 我去刷会手机..." << std::endl;

    // 4. 刷完手机，我要取餐了！
    // 【关键动作】如果咖啡还没好，get() 会【自动阻塞睡眠】，直到后厨 set_value
    // 不需要写锁，不需要写 wait，干干净净！
    std::string coffee = receipt.get(); 
    std::cout << "[顾客] 拿到了：" << coffee << std::endl;

    t.join();

    std::cout << sizeof(order_promise) << '\n';

    return 0;
}