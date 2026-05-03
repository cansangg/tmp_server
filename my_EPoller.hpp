#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>
#include "my_TcpSocket.hpp"

namespace my {
    class EPoller {
    private:
        int epfd;
        // 用 unordered_map 替代 set，完美解决 O(1) 查找和可以修改元素的问题
        // Key: fd, Value: TcpSocket 对象本身
    public:
        std::unordered_map<int, my::TcpSocket> m_sockets; 

    public: 
        EPoller() {
            epfd = epoll_create1(0);
        }

        ~EPoller() {
            if (epfd >= 0) close(epfd);
        }

        // ==========================================
        // 核心接口：添加 Socket
        // 无论是 Server 还是 Client，直接往里塞就行！
        // ==========================================
        void addSocket(my::TcpSocket&& sock) {
            int fd = sock.getFd();
            
            // 挂载到 epoll 红黑树
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
            
            // 存入 map 中管理生命周期 (使用 std::move 转移所有权)
            m_sockets.emplace(fd, std::move(sock));
        }

        // ==========================================
        // 核心接口：移除 Socket
        // ==========================================
        void removeSocket(int fd) {
            // 当 map.erase 被调用时，TcpSocket 触发析构，调用 close(fd)。
            // Linux 极其智能，底层 fd 关闭时，会自动从 epoll 的红黑树上剔除，无需 epoll_ctl DEL！
            m_sockets.erase(fd); 
        }

        // ==========================================
        // 极其干净的 poll：只传时间，没有任何业务回调！
        // ==========================================
        void poll(int timeout_ms) {
            const int MAX_EVENTS = 1024;
            struct epoll_event events[MAX_EVENTS];

            int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                m_sockets[fd].handle_event();
            }
        }
    };
}