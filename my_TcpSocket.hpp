#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <optional>
#include <functional>
#include <netinet/tcp.h>

// size_t: 3
// fd_kernel_buffer: {'U', 'R', EOF} 

namespace my {
    class TcpSocket { 
    private:
        int fd;
        std::string in_buffer;
        bool private_is_closed = false;

        explicit TcpSocket(int client_fd) : fd(client_fd) {
            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)); //tcp包不打成大包发出
            setBlocking(false); //默认非堵塞防半包时readExactly堵很久
        }

        bool recv_to_buffer() {
            if (fd < 0 || private_is_closed) return false;
            char tmp_buf[4096];
            int bytes_read = ::read(fd, tmp_buf, sizeof(tmp_buf));
            
            if (bytes_read > 0) {
                // 【情况1：健康】成功拿到数据
                in_buffer.append(tmp_buf, bytes_read);
                return true;
            } else if (bytes_read == 0) {
                // 【情况2：和平分手】收到对端的 FIN 包 (EOF)
                private_is_closed = true;
                return false;
            } else /*if (bytes_read < 0)*/ {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // **【情况3：暂无数据】这是非阻塞才有的正常现象，连接没死！**
                    return false; 
                }
                // 【情况4：意外暴毙】比如收到 RST 重置包
                private_is_closed = true;
                return false;
            }
        }

        void close() {
            if (fd != -1) {
                ::close(fd);
                fd = -1;
                in_buffer.clear();
            }
        }

    public:
        std::function<void(my::TcpSocket*)> handle_event;

    public:
        TcpSocket() {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Socket 创建失败!");
            
            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //(待填坑tcp挥手)
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)); //tcp包不打成大包发出

            setBlocking(false); //默认非堵塞防半包时readExactly堵很久(作为接客fd时也要让accept非堵塞防RST)
        }

        ~TcpSocket() { close(); }

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;
        
        // 1. 补全移动构造函数
        TcpSocket(TcpSocket&& other) noexcept : 
            fd(other.fd), 
            in_buffer(std::move(other.in_buffer)),
            handle_event(std::move(other.handle_event)),
            private_is_closed(other.private_is_closed)
        {
            other.fd = -1;
            other.private_is_closed = true;
        }

        // 2. 补全移动赋值运算符
        TcpSocket& operator=(TcpSocket&& other) noexcept {
            if (this != &other) {
                close();
                fd = other.fd;
                in_buffer = std::move(other.in_buffer);
                handle_event = std::move(other.handle_event);
                private_is_closed = other.private_is_closed;
                other.fd = -1;
                other.private_is_closed = true;
            }
            return *this;
        }

        void connectTo(const std::string& host, int port) {
            struct hostent* he = gethostbyname(host.c_str());
            if (he == nullptr) throw std::runtime_error("域名解析失败: " + host);
            
            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr = *(struct in_addr*)he->h_addr_list[0];

            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1) throw std::runtime_error("fcntl 获取状态失败");
            bool is_originally_blocking = (flags & O_NONBLOCK) == 0;

            setBlocking(true); 
            
            int connect_err = 0;
            if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                connect_err = errno;
            }

            setBlocking(is_originally_blocking);

            if (connect_err != 0) {
                throw std::runtime_error(std::string("连接服务器失败: ") + strerror(connect_err));
            }
        }

        void bindAndListen(int port, int backlog = 128) {
            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
                throw std::runtime_error("Bind 失败!");
            if (::listen(fd, backlog) < 0)
                throw std::runtime_error("Listen 失败!");
        }


        //std::nullopt: 非堵塞读取,暂无数据
        //std::optional<TcpSocket{}>：读到数据
        std::optional<TcpSocket> acceptClient() {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &len); //先EPoller提示响了(或主动摸)，再::accept()尝试去非堵塞读
            
            if (client_fd < 0) {
                // 【常态 1】：非阻塞队列被抽干了，门外没客人了
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return std::nullopt; // 返回一个优雅的“空盒子”
                }
                // 【常态 2】：幽灵 RST，客人连上又瞬间跑路了
                if (errno == ECONNABORTED) {
                    return std::nullopt;
                }
                // 【常态 3】：系统调用被底层的某些信号打断了
                if (errno == EINTR) {
                    return std::nullopt;
                }

                throw std::runtime_error(std::string("Accept 遭遇错误: ") + strerror(errno));
            }
            
            return TcpSocket(client_fd); // 自动包装成 optional 成功态
        }

        //std::nullopt: 关闭连接
        //std::optional<"">：非堵塞读取,暂无数据
        //std::optional<"LRU">：读到数据
        std::optional<std::string> readExactly(size_t length) {
            while (in_buffer.length() < length) {
                if (!recv_to_buffer()) {
                    if (private_is_closed) return std::nullopt;
                    else return ""; 
                }
            }
            std::string result = in_buffer.substr(0, length);
            in_buffer.erase(0, length);
            return result;
        }

        //http协议用readUntil("\r\n\r\n")实现瞄一眼效果
        std::optional<std::string> readUntil(const std::string& delimiter) {
            while (true) {
                size_t pos = in_buffer.find(delimiter);
                if (pos != std::string::npos) {
                    size_t extract_len = pos + delimiter.length();
                    std::string result = in_buffer.substr(0, extract_len);
                    in_buffer.erase(0, extract_len); 
                    return result;
                }
                if (!recv_to_buffer()) {
                    if (private_is_closed) return std::nullopt;
                    else return ""; 
                }
            }
        }

        bool write(const std::string& msg) {
            if (fd < 0) return false;
            ssize_t sent = ::send(fd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
            return sent == (ssize_t)msg.length();
        }

        int getFd() const { return fd; }

        void setBlocking(bool blocking) { //设置::read(this->fd)时非堵塞
            if (fd < 0 || private_is_closed) return;
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1) throw std::runtime_error("fcntl F_GETFL 失败");

            if (!blocking) {
                flags |= O_NONBLOCK;  
            } else {
                flags &= ~O_NONBLOCK; 
            }

            if (fcntl(fd, F_SETFL, flags) == -1) {
                throw std::runtime_error("fcntl F_SETFL 失败");
            }
        }

        void setHandleEvent(std::function<void(my::TcpSocket*)> _handle_event) {
            handle_event = std::move(_handle_event);
        };

        static size_t getBodyLength(const std::string& header) {
            size_t pos = header.find("Content-Length:");
            if (pos == std::string::npos) pos = header.find("content-length:");

            if (pos != std::string::npos) {
                pos += 15; 
                size_t end_pos = header.find("\r\n", pos);
                
                if (end_pos != std::string::npos) {
                    try {
                        std::string num_str = header.substr(pos, end_pos - pos);
                        return std::stoull(num_str);
                    } catch (const std::exception&) {
                        return 0;
                    }
                }
            }
            return 0; 
        }
    };
}

// 值传递 = 必须在函数内部构造一个新对象。至于怎么构造？既可以是拷贝构造，也可以是移动构造
// 传右值时，值传递会引发移动（只要你写了移动构造）。
// const T& 传右值也只会拷贝构造
// 函数传参一定有新对象(形参)的建立