#include "my_atomic_mutex_semaphore.hpp"

#include <iostream>
#include <vector>
#include <thread>

my::semaphore s1(1), s2(0), s3(0);

int main() {
	std::vector<std::thread> v;
	v.emplace_back([&](){
		int cnt = 10;
        while (cnt--) {
            s1.acquire();
            std::cout << "first\n";
            s2.release();
        }
	});

    v.emplace_back([&](){
		int cnt = 10;
        while (cnt--) {
            s2.acquire();
            std::cout << "second\n";
            s3.release();
        }
	});

    v.emplace_back([&](){
		int cnt = 10;
        while (cnt--) {
            s3.acquire();
            std::cout << "third\n";
            s1.release();
        }
	});

    for (std::thread& t : v) {
        t.join();
    }
	
	return 0;
}