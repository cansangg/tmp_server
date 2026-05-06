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

// size_t: 2
// fd_kernel_buffer: {'U', 'R', (EOF_FLAG/RST_FLAG <- notrealdata)} 

/*核心接口：
    堵塞式: connectTo()
    非堵塞式: bindAndListen(), setBlocking(), setHandleEvent(), setHandleWrite(), getFd(),
        两态acceptClient()，三态readExactly()/readUntil()
*/

namespace my {
    class TcpSocket { 
    private:
        int fd;
        std::string in_buffer;
        std::string out_buffer;

        explicit TcpSocket(int client_fd) : fd(client_fd) {
            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)); //tcp包不打成大包发出
            setBlocking(false); //默认非堵塞防半包时readExactly堵很久
        }

        ssize_t recv_to_buffer() { // 大于0 : 成功吸入的字节数 0 : 连接关闭 (正常EOF/异常RST) -1 : 暂无数据 (EAGAIN)
            if (fd < 0) return 0;
            char tmp_buf[4096];
            ssize_t bytes_read = ::read(fd, tmp_buf, sizeof(tmp_buf));
            
            if (bytes_read > 0) {
                in_buffer.append(tmp_buf, bytes_read);
                return bytes_read; //读到数据
            } else if (bytes_read == 0) {
                return 0; // 正常EOF
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return -1; // 暂无数据
                }
                // 异常RST (用户直接拔网线/杀进程了)
                return 0; 
            }
        }

        void close() {
            if (fd != -1) {
                ::close(fd);
                fd = -1;
                in_buffer.clear();
                out_buffer.clear();
            }
        }

    public:
        bool write_waiting = false; //避免频繁系统调用
        std::function<void(my::TcpSocket*)> handle_event;
        std::function<void(my::TcpSocket*)> handle_write;

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
            write_waiting(other.write_waiting),
            in_buffer(std::move(other.in_buffer)),
            out_buffer(std::move(other.out_buffer)),
            handle_event(std::move(other.handle_event)),
            handle_write(std::move(other.handle_write))
        {
            other.fd = -1;
        }

        // 2. 补全移动赋值运算符
        TcpSocket& operator=(TcpSocket&& other) noexcept {
            if (this != &other) {
                close();
                fd = other.fd;
                write_waiting = other.write_waiting;
                in_buffer = std::move(other.in_buffer);
                out_buffer = std::move(other.out_buffer);
                handle_event = std::move(other.handle_event);
                handle_write = std::move(other.handle_write);
                other.fd = -1;
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

            setBlocking(true); //拨号逻辑上应堵塞
            
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
        //LISTEN 状态, 无关闭连接语义
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

                throw std::runtime_error(std::string("Accept 遭遇错误: ") + strerror(errno));
            }
            
            return TcpSocket(client_fd); // 自动包装成 optional 成功态
        }

        //std::nullopt: 非堵塞读取,暂无数据
        //std::optional<"">：关闭连接
        //std::optional<"LRU">：读到数据
        std::optional<std::string> readExactly(size_t length) {
            while (in_buffer.length() < length) {
                ssize_t res = recv_to_buffer();
                if (res == 0) return "";
                if (res == -1) return std::nullopt;
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
                ssize_t res = recv_to_buffer();
                if (res == 0) return "";
                if (res == -1) return std::nullopt;
            }
        }

        void write(const std::string& msg) {
            if (fd < 0 || msg.empty()) return;
            out_buffer += msg;
            if (handle_write) handle_write(this);
            return;
        }

        //1: 发完了 0: 没发完 -1: 连接断开
        ssize_t send_to_kernel() {
            ssize_t sent = ::send(fd, out_buffer.c_str(), out_buffer.length(), MSG_NOSIGNAL);
            if (sent > 0) { //发了一部分
                out_buffer.erase(0, sent);
                return out_buffer.empty();
            } else /*if (sent < 0)*/ {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return 0; //只是内核满了
                }
                return -1; // 遭遇RST, poller响后在client->handle_event()里处理断开连接
            }
        }

        int getFd() const { return fd; }

        void setBlocking(bool blocking) { //设置::read(this->fd)/::accept(this->fd)时非堵塞
            if (fd < 0) return;
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

        void setHandleWrite(std::function<void(my::TcpSocket*)> _handle_write) {
            handle_write = std::move(_handle_write);
        };
    };

    size_t getBodyLength(const std::string& header) {
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
}

// 值传递 = 必须在函数内部构造一个新对象。至于怎么构造？既可以是拷贝构造，也可以是移动构造
// 传右值时，值传递会引发移动（只要你写了移动构造）。
// const T& 传右值也只会拷贝构造
// 函数传参一定有新对象(形参)的建立