#include <iostream>
#include <thread>
#include ""
#include <chrono>
#include <vector>

// 1. 定义信号量 (直接对应之前的 PV 分析)
// 互斥信号量：井和缸一次只能一个人用，用二元信号量
std::binary_semaphore mutex1{1}; 
std::binary_semaphore mutex2{1};  

// 资源/同步信号量：用计数信号量
// 模板参数填最大可能值，这里水桶最多3个，缸容量最多10
std::counting_semaphore<3> count{3}; 
std::counting_semaphore<10> empty{10};  
std::counting_semaphore<10> full{0};    

// 模拟小和尚（生产者）
void little_monk(int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 休息一会儿再打
    // --- 准备阶段 ---
    empty.acquire();    // P(empty): 检查缸里有没有空位置
    count.acquire(); // P(count): 抢一个空水桶

    // --- 打水阶段 ---
    mutex1.acquire();   // P(mutex1): 锁住水井
    std::cout << "小和尚 [" << id << "] 正在井边打水...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 模拟动作耗时
    mutex1.release();   // V(mutex1): 离开水井

    // --- 倒水阶段 ---
    mutex2.acquire();    // P(mutex2): 锁住水缸
    std::cout << "小和尚 [" << id << "] 把水倒入水缸。\n";
    mutex2.release();    // V(mutex2): 离开水缸

    // --- 结束阶段 ---
    count.release(); // V(count): 还回水桶
    full.release();     // V(full): 缸里的水多了一桶，唤醒老和尚
}

// 模拟老和尚（消费者）
void old_monk(int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 休息一会儿再打
    // --- 准备阶段 ---
    full.acquire();     // P(full): 检查缸里有没有水
    count.acquire(); // P(count): 抢一个空水桶准备舀水

    // --- 舀水阶段 ---
    mutex2.acquire();    // P(mutex2): 锁住水缸
    std::cout << "  -> 老和尚 (" << id << ") 从水缸舀了一桶水。\n";
    mutex2.release();    // V(mutex2): 离开水缸

    // --- 喝水阶段 ---
    std::cout << "  -> 老和尚 (" << id << ") 正在开心喝水...\n";

    // --- 结束阶段 ---
    count.release(); // V(count): 还回水桶
    empty.release();    // V(empty): 缸里空出了一个位置，唤醒小和尚
}

int main() {
    std::cout << "寺庙打水系统启动...\n";

    std::vector<std::thread> monks;

    // 雇佣 3 个小和尚打水
    for (int i = 1; i <= 3; ++i) {
        monks.emplace_back(little_monk, i);
    }

    // 雇佣 2 个老和尚喝水
    for (int i = 1; i <= 2; ++i) {
        monks.emplace_back(old_monk, i);
    }

    // 挂起主线程，让和尚们一直干活
    for (auto& t : monks) {
        t.join();
    }

    return 0;
}