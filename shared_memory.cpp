#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <wait.h>
#include <sys/syscall.h>
#include <linux/futex.h>

namespace std {
    template <typename T>
    class atomic {
    private:
        // volatile 极其关键！警告编译器：不要把这个变量优化进寄存器，必须每次都去内存里读！
        volatile T v; 

    public:
        atomic(T v = T{}) : v(v) {}

        bool compare_exchange(T expected, T new_value) {
            bool result;
            
            // 💣 纯正的内联汇编，直接指挥 CPU
            __asm__ __volatile__(
                "lock cmpxchg %2, %1 \n\t"  // 核心指令：如果 %1(内存) == eax，就把 %2 塞进 %1
                "setz %0"                   // 如果上一步相等，CPU的零标志位(ZF)会变1，这行代码把 ZF 提出来赋给 result
                
                // --- 下面是与编译器的契约 ---
                : "=q" (result), "+m" (v)   // 输出：result 存在任意寄存器，value 是内存且会被修改
                : "r" (new_value), "a" (expected) // 输入：new_value 随便找个寄存器，expected 必须死死绑在 eax 寄存器上！
                : "cc", "memory"                  // 警告编译器：我改了条件状态码(cc)，且内存被我暗改了(memory)，你别给我搞代码重排！
            );
            
            return result;
        }

        T load() const {
            return v;
        }
    };


    class mutex {
    private:
        // 0 代表没锁，1 代表被锁
        atomic<int> flag{0}; //可认为atomic成员函数都是单条汇编指令&&不会多核同时执行

    public:
        void lock() { //会堵塞进程
            int cnt = 0;
            while (++cnt >= 100 || flag.compare_exchange(0, 1)) { //先自旋，零系统开销
                return;
            }

            while (true) { //再互斥
                // 【移交内核，挂起线程】
                // syscall 需要的是真实的物理地址指针。
                // 因为 atomic<int> 里唯一的成员就是 volatile int v，
                // 所以强转 (int*)&flag 就完美等价于底层那个 4 字节的 flag 地址。
                // 翻译：内核大哥，如果 flag 现在还是 1，就请剥夺我的 CPU 时间片，把我塞进红黑树等候室。
                syscall(SYS_futex, (int*)&flag, FUTEX_WAIT, 1, nullptr, nullptr, 0);
                
                // 每次睡醒时，再试着抢一次
                if (flag.compare_exchange(0, 1)) {
                    break; // 抢到了，跳出循环去干活
                }
            }
        }

        void unlock() {
            // 【释放锁】
            flag.compare_exchange(1, 0);

            // 【唤醒兄弟】
            // 翻译：内核大哥，锁我已经改成 0 了。去你的等待队列里，随便踢醒 1 个排队的兄弟。
            syscall(SYS_futex, (int*)&flag, FUTEX_WAKE, 1, nullptr, nullptr, 0);
        }
    };
}

int main() {
    // 1. 在共享内存区申请一个 int 大小的空间
    // MAP_SHARED 表示对内存的修改对其他进程可见
    // MAP_ANONYMOUS 表示不关联文件，只在 RAM 里
    int* cnt = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    std::mutex* mtx = new (mmap(NULL, sizeof(std::mutex), PROT_READ | PROT_WRITE, 
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0)) std::mutex();
    
    *cnt = 0; // 初始化

    if (fork() == 0) { // 子进程
        for(int i = 0; i < 100000; ++i) mtx->lock(), (*cnt)++, mtx->unlock(); 
        return 0;
    } else { // 父进程
        for(int i = 0; i < 100000; ++i) mtx->lock(), (*cnt)++, mtx->unlock(); 
        wait(NULL); // 等子进程结束
        std::cout << "Final cnt: " << *cnt << std::endl; 
        // 结果大概率小于 200000，因为没加锁！
    }

    munmap(cnt, sizeof(int)); // 释放内存
    munmap(mtx, sizeof(std::mutex)); // 释放内存
    return 0;
}