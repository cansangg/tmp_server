#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>
#include "my_TcpSocket.hpp"

namespace my {
    class EPoller {
    private:
        int epfd;
    public:
        std::unordered_map<int, my::TcpSocket> m_sockets; 

    public: 
        EPoller() {
            epfd = epoll_create1(0);
        }

        ~EPoller() {
            if (epfd >= 0) close(epfd);
        }

        void addSocket(my::TcpSocket&& sock) {
            int fd = sock.getFd();
            
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
            
            m_sockets.emplace(fd, std::move(sock));
        }

        void removeSocket(int fd) {
            // 当 map.erase 被调用时，TcpSocket 触发析构，调用 close(fd)。
            // Linux底层 fd 关闭时，会自动从 epoll 的红黑树上剔除，无需 epoll_ctl DEL！
            m_sockets.erase(fd); 
        }

        void poll(int timeout_ms) {
            const int MAX_EVENTS = 1024;
            struct epoll_event events[MAX_EVENTS];

            int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                m_sockets[fd].handle_event(&m_sockets[fd]);
            }
        }
    };
}