#include <iostream>
#include <string>
#include <unordered_set>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

const int PORT = 8080;
const int MAX_EVENTS = 100; // epoll_wait 每次最多拿几个事件
const int BUF_SIZE = 1024;

int main() {
    // 1. 创建服务器大门：监听 Socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // 设置端口复用（防止重启服务器时报错“端口被占用”）
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 绑定端口并开始监听
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }
    listen(listen_fd, SOMAXCONN);
    std::cout << "🚀 聊天室服务器已启动，监听端口: " << PORT << std::endl;

    // ==========================================
    // 核心开始：epoll 登场
    // ==========================================

    // 2. 创建 epoll 实例（建立外包公司）
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1 failed");
        return -1;
    }

    // 3. 把大门（listen_fd）挂到 epoll 的红黑树上
    struct epoll_event ev;
    ev.events = EPOLLIN; // 我们只关心“可读”事件（有新连接来了）
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    // 用一个集合保存所有已连接的客户 fd，方便群发
    std::unordered_set<int> client_fds;
    struct epoll_event events[MAX_EVENTS]; // 用来接收就绪事件的篮子

    // 4. 死循环：坐等事件送上门
    while (true) {
        // 挂起线程，直到就绪链表里有货
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; ++i) {
            int active_fd = events[i].data.fd;

            // 情况 A：大门响了（有新客户连接）
            if (active_fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int new_client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (new_client_fd >= 0) {
                    std::cout << "[新连接] 客户端 FD: " << new_client_fd 
                              << " IP: " << inet_ntoa(client_addr.sin_addr) << std::endl;
                    
                    // 把新客户也挂到 epoll 上，盯着他说话
                    ev.events = EPOLLIN;
                    ev.data.fd = new_client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, new_client_fd, &ev);
                    
                    client_fds.insert(new_client_fd); // 加入群聊名单

                    // 发送欢迎语
                    std::string welcome = "欢迎加入黑客聊天室！你的代号是 FD " + std::to_string(new_client_fd) + "\n";
                    send(new_client_fd, welcome.c_str(), welcome.length(), 0);
                }
            } 
            // 情况 B：桌子响了（某个老客户发消息了，或者退出了）
            else {
                char buffer[BUF_SIZE];
                memset(buffer, 0, BUF_SIZE);
                int bytes_read = read(active_fd, buffer, BUF_SIZE - 1);

                if (bytes_read <= 0) {
                    // 读到 0 字节，说明客户端断开了连接
                    std::cout << "[掉线] 客户端 FD: " << active_fd << " 已退出群聊。\n";
                    
                    // 收尾工作：从 epoll 树上摘除，关闭 fd，踢出群聊名单
                    epoll_ctl(epfd, EPOLL_CTL_DEL, active_fd, NULL);
                    close(active_fd);
                    client_fds.erase(active_fd);
                } else {
                    // 收到正常消息，准备群发（广播）
                    std::cout << "[收到消息] 来自 FD " << active_fd << ": " << buffer;

                    std::string broadcast_msg = "[FD " + std::to_string(active_fd) + " 说]: " + buffer;
                    
                    // 遍历所有客户，除了发消息的本人，其他人都发一份
                    for (int other_fd : client_fds) {
                        if (other_fd != active_fd) {
                            send(other_fd, broadcast_msg.c_str(), broadcast_msg.length(), 0);
                        }
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epfd);
    return 0;
}