#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <climits>

namespace my {
    template <typename T>
    class atomic {
    private:
        volatile T v; 

    public:
        static_assert(sizeof(T) == 4, "Futex syscall MUST strictly operate on 32-bit integers (4 bytes)!");

        atomic(T v = T{}) : v(v) {}

        bool compare_exchange(T expected, T new_value) {
            bool result; 
            __asm__ __volatile__("lock cmpxchg %2, %1 \n\t" "setz %0" : "=q" (result), "+m" (v) : "r" (new_value), "a" (expected) : "cc", "memory");
            return result;
        }

        void fetch_add(T x) {
            while (true) {
                T pre = v;
                T new_value = pre + x;
                if (compare_exchange(pre, new_value)) break;
            }
        }

        void wait_until(T target) {
            while (true) {
                T pre = v;
                if (pre == target) break;
                syscall(SYS_futex, (int*)&v, FUTEX_WAIT, pre, nullptr, nullptr, 0);
            }
        }

        void wait_if(T target) {
            if (v == target) syscall(SYS_futex, (int*)&v, FUTEX_WAIT, target, nullptr, nullptr, 0);
        }

        void wake(int num = INT_MAX) {
            syscall(SYS_futex, (int*)&v, FUTEX_WAKE, num, nullptr, nullptr, 0);
        }

        void store(T new_value) { v = new_value; } //参数为编译时常量时为原子操作

        T load() const { return v; }
    };


    class mutex {
    private:
        atomic<int> ato;

    public:
        mutex(int state = 1) : ato(state != 0) {}

        void lock() {
            int cnt = 0;
            while (++cnt <= 100) if (ato.load() == 0 && ato.compare_exchange(0, 1)) return;
            while (true) {
                ato.wait_until(0);
                if (ato.compare_exchange(0, 1)) break;
            }
        }

        void unlock() {
            ato.store(0);
            ato.wake(1);
        }
    };

    class semaphore {
    private:
        atomic<int> ato;

    public:
        semaphore(int capacity) : ato(capacity) {}

        void acquire() {
            while (true) {
                int pre = ato.load();
                if (pre > 0) {
                    if (ato.compare_exchange(pre, pre - 1)) break;
                } else {
                    ato.wait_if(pre);
                }
            }
        }

        void release() {
            ato.fetch_add(1);
            ato.wake(1);
        }
    };
}