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

// ==================== 基础 Socket 封装 ====================
int get_tcp_fd() { return socket(AF_INET, SOCK_STREAM, 0); }
void bind_port(int fd, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));
}
void start_listening(int fd) { listen(fd, 128); }
int wait_for_new_client(int listen_fd) { return accept(listen_fd, nullptr, nullptr); }
// ==========================================================

// ==================== 你的私人 epoll 封装库 ====================

// 1. 创建前台的“呼叫器屏幕” (epoll 实例)
int create_epoll() {
    // epoll_create1(0) 是较新的 API，推荐使用
    return epoll_create1(0); 
}

// 2. 把指定的 fd 挂到呼叫器上监视
void add_to_epoll(int epoll_fd, int target_fd) {
    struct epoll_event event;
    event.data.fd = target_fd;    // 记录这桌的号码 (fd)
    event.events = EPOLLIN;       // 监视的事件：EPOLLIN 表示“有数据进来可读”
    // 注意：如果是生产级，这里通常会加一个 EPOLLET (边缘触发) 标志，并配合非阻塞 IO
    
    // 把事件注册到底层的红黑树上
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, target_fd, &event);
}

// 3. 把指定的 fd 从呼叫器上摘除 (客人走了)
void remove_from_epoll(int epoll_fd, int target_fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, target_fd, nullptr);
}

// ==========================================================

#define MAX_EVENTS 1024 // 每次最多从屏幕上同时看多少个响铃的桌子

int main() {
    int listen_fd = get_tcp_fd();
    bind_port(listen_fd, 8080);
    start_listening(listen_fd);
    
    // 1. 买一个前台呼叫器屏幕
    int epoll_fd = create_epoll();
    
    // 2. 最关键的一步：把保安 (listen_fd) 也挂到呼叫器上监视！
    // 只要有新客人来，保安就会按铃。
    add_to_epoll(epoll_fd, listen_fd);
    
    // 准备一个小本子，用来记录每次哪些桌子按铃了
    struct epoll_event events[MAX_EVENTS];

    printf("🚀 Epoll 高并发聊天室启动！\n");

    while (true) {
        // 3. 服务员死盯着屏幕，等铃响。
        // epoll_wait 会阻塞在这里休眠，直到有任何一个或多个事件发生才醒来。
        // 返回值 active_count 就是当前有几桌按了铃。
        int active_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        // 醒来干活！挨个处理响铃的桌子
        for (int i = 0; i < active_count; i++) {
            int current_fd = events[i].data.fd; // 看看是哪个号码响的铃
            
            // 场景 A：响铃的是保安 (listen_fd)
            // 说明有新客人要连进大厅了
            if (current_fd == listen_fd) {
                int new_chat_fd = wait_for_new_client(listen_fd);
                printf(">> 有新用户接入聊天室，分配 fd: %d\n", new_chat_fd);
                
                // 给新客人发个手机，并把他的手机号挂到呼叫器上监视
                add_to_epoll(epoll_fd, new_chat_fd);
            } 
            // 场景 B：响铃的是某个老客人 (chat_fd)
            // 说明他在网页上发消息了，或者他在拉取历史记录
            else {
                char buf[4096];
                memset(buf, 0, sizeof(buf));
                
                int bytes_read = read(current_fd, buf, sizeof(buf) - 1);
                
                // 如果读到了 0 字节，说明客人把浏览器关了（TCP 断开连接机制）
                if (bytes_read <= 0) {
                    printf(">> 用户 (fd: %d) 离开了聊天室。\n", current_fd);
                    remove_from_epoll(epoll_fd, current_fd); // 从屏幕上摘除
                    close(current_fd); // 没收手机
                } 
                else {
                    // 他发数据过来了！这里放你之前的“大长串 HTTP 报文读取器”
                    // 并且处理完之后，不用写close(current_fd); 
                    // 这样他下次点发送时，依然会触发 epoll_wait 醒来
                    
                    // ... 你的业务逻辑：处理 request_data，write 回应 ...
                    std::string s;
                    char buf[1024];
                    int bytes_read;
                    while (bytes_read = read(current_fd, buf, 1023)) {
                        buf[bytes_read] = '\0';
                        s += buf;
                    }
                    s += '0' + current_fd;
                    write(current_fd, s.c_str(), s.size());
                }
            }
        }
    }
    
    return 0;
}