#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include "my_TcpSocket.hpp"

namespace my {
    class EPoller { //effective轮询机？^_^
    private:
        int epfd;
    public:
        std::unordered_map<int, my::TcpSocket> m_sockets; 
        std::vector<int> to_remove;

    public: 
        EPoller() {
            epfd = epoll_create1(0);
        }

        EPoller(const EPoller&) = delete;
        EPoller& operator=(const EPoller&) = delete;
        EPoller(EPoller&&) = delete;
        EPoller& operator=(EPoller&&) = delete;
        ~EPoller() {
            if (epfd >= 0) close(epfd);
        }

        void addSocket(my::TcpSocket&& client) {
            int fd = client.getFd();
            
            struct epoll_event ev;
            ev.events = EPOLLIN; //默认EPOLLIN
            ev.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

            m_sockets.emplace(fd, std::move(client));
        }

        void removeSocketLazy(int fd) { //懒删除
            to_remove.push_back(fd);
        }

        void modifySocket(int fd, uint32_t events) {
            if (fd < 0) return;

            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = events;

            epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        }

        void poll(int timeout_ms) {
            const int MAX_EVENTS = 1024;
            struct epoll_event events[MAX_EVENTS];

            int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                uint32_t ev = events[i].events;
                if (ev & EPOLLOUT) m_sockets[fd].handle_write(&m_sockets[fd]); //内核写缓冲区有空位了，继续写
                if (ev & EPOLLIN) m_sockets[fd].handle_read(&m_sockets[fd]); //TcpSocket响了自己在回调函数里读并处理
            }

            for (int fd : to_remove) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                m_sockets.erase(fd); 
            }
            to_remove.clear();
        }
    };
}

std::function {
    T lambda;
    operator () {
        if (lambda) lambda();
    }
};

Poller {
    vector<std::function> v;
}

int main() {
    Poller poller;
    {
        class Lambda {
            Poller* p = &poller;
            void operator() {
                p->v.clear();
                std::cout << p->v.size() << '\n';
            }
        } lambda;
        poller.v.push_back(lambda);
    }
    
}