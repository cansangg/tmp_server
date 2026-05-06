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

        ~EPoller() {
            if (epfd >= 0) close(epfd);
        }

        void addSocket(my::TcpSocket&& client) {
            int fd = client.getFd();

            //client->write()和poller->poll()里调用handle_write
            client.setHandleWrite([this](my::TcpSocket* cli) -> void {
                int fd = cli->getFd();
                ssize_t ok = cli->send_to_kernel();
                if (ok != 1) { 
                    if (!cli->write_waiting) {
                        cli->write_waiting = true;
                        struct epoll_event ev;
                        ev.events = EPOLLIN | EPOLLOUT; //加入写监听,若连接断开无影响,EPOLLIN会响并懒删除
                        ev.data.fd = fd;
                        epoll_ctl(this->epfd, EPOLL_CTL_MOD, fd, &ev);
                    }
                }

                if (ok == 1) {
                    if (cli->write_waiting) {
                        cli->write_waiting = false;
                        struct epoll_event ev;
                        ev.events = EPOLLIN; //取消写监听
                        ev.data.fd = fd;
                        epoll_ctl(this->epfd, EPOLL_CTL_MOD, fd, &ev);
                    }
                }
            });
            
            struct epoll_event ev;
            ev.events = EPOLLIN; //EPOLLIN
            ev.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
            
            m_sockets.emplace(fd, std::move(client));
        }

        void removeSocketLazy(int fd) { //懒删除
            to_remove.push_back(fd);
        }

        void poll(int timeout_ms) {
            const int MAX_EVENTS = 1024;
            struct epoll_event events[MAX_EVENTS];

            int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                uint32_t ev = events[i].events;
                if (ev & EPOLLOUT) m_sockets[fd].handle_write(&m_sockets[fd]); //内核写缓冲区有空位了，继续写
                if (ev & EPOLLIN) m_sockets[fd].handle_event(&m_sockets[fd]); //TcpSocket响了自己在回调函数里读并处理
            }

            for (int fd : to_remove) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                m_sockets.erase(fd); 
            }
            to_remove.clear();
        }
    };
}