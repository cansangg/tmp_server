#include <vector>
#include <algorithm>
#include <functional>
#include <sys/select.h>
#include <cstddef>
#include "my_TcpSocket.hpp"

namespace my {
    class SelectPoller {
    public: 
        // ==========================================
        // 简单粗暴，全部 Public！想怎么摸就怎么摸
        // ==========================================
        my::TcpSocket m_server;          // 内部直接持有的服务端对象
        std::vector<my::TcpSocket> m_clients;  // 直接暴露的客户端大军

        // 构造时，直接传入端口号，内部帮你把大门建好！
        explicit SelectPoller(int port) {
            m_server.bindAndListen(port);
        }

        // ==========================================
        // 核心巡逻函数
        // ==========================================
        void poll(int timeout_ms, 
                  std::function<void()> onNewConnection, 
                  std::function<void(my::TcpSocket&)> onClientData) 
        {
            fd_set readfds;
            FD_ZERO(&readfds);           
            
            int listen_fd = m_server.getFd();
            FD_SET(listen_fd, &readfds); 
            int max_fd = listen_fd;

            for (const auto& client : m_clients) {    
                int fd = client.getFd();
                FD_SET(fd, &readfds);
                if (fd > max_fd) max_fd = fd;
            }

            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            select(max_fd + 1, &readfds, NULL, NULL, &tv);

            // 1. 大门来客
            if (FD_ISSET(listen_fd, &readfds)) {
                onNewConnection();
            }

            // 2. 客人有需求 (先记录谁响了，防止迭代器在回调中失效)
            std::vector<int> active_fds;
            for (const auto& client : m_clients) {
                if (FD_ISSET(client.getFd(), &readfds)) {
                    active_fds.push_back(client.getFd());
                }
            }

            for (int fd : active_fds) {
                auto it = std::find_if(m_clients.begin(), m_clients.end(),
                    [fd](const my::TcpSocket& s) { return s.getFd() == fd; });
                
                if (it != m_clients.end()) {
                    onClientData(*it); 
                }
            }
        }
    };
}