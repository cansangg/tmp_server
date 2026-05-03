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

namespace my {
    class TcpSocket { 
    private:
        int fd;
        std::string in_buffer;

        explicit TcpSocket(int client_fd) : fd(client_fd) {}

        bool recv_to_buffer() {
            if (fd < 0) return false;
            char tmp_buf[4096];
            int bytes_read = ::read(fd, tmp_buf, sizeof(tmp_buf));
            if (bytes_read <= 0) return false;
            in_buffer.append(tmp_buf, bytes_read);
            return true;
        }
        
    public:
        std::function<void(my::TcpSocket*)> handle_event;

    public:
        TcpSocket() {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) throw std::runtime_error("Socket 创建失败!");
            
            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        }

        ~TcpSocket() { close(); }

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;
        
        // 1. 补全移动构造函数
        TcpSocket(TcpSocket&& other) noexcept : 
            fd(other.fd), 
            in_buffer(std::move(other.in_buffer)),
            handle_event(std::move(other.handle_event)) // <== 救命稻草在这里！
        {
            other.fd = -1;
        }

        // 2. 补全移动赋值运算符
        TcpSocket& operator=(TcpSocket&& other) noexcept {
            if (this != &other) {
                close();
                fd = other.fd;
                in_buffer = std::move(other.in_buffer);
                handle_event = std::move(other.handle_event); // <== 还有这里！
                other.fd = -1;
            }
            return *this;
        }

        void connectTo(const std::string& host, int port) { //客户端函数
            struct hostent* he = gethostbyname(host.c_str());
            if (he == nullptr) throw std::runtime_error("域名解析失败: " + host);
            
            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr = *(struct in_addr*)he->h_addr_list[0];

            if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                throw std::runtime_error("连接服务器失败!");
            }
        }

        void bindAndListen(int port, int backlog = 128) { //服务端函数
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

        TcpSocket acceptClient() {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &len);
            if (client_fd < 0) throw std::runtime_error("Accept 失败!");
            
            return TcpSocket(client_fd); // 这里悄悄调用了上面那个私有构造函数
        }

        bool write(const std::string& msg) {
            if (fd < 0) return false;
            ssize_t sent = ::send(fd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
            return sent == (ssize_t)msg.length();
        }

        std::string read() {
            if (in_buffer.empty() && !recv_to_buffer()) return std::string();
            std::string tmp;
            swap(tmp, in_buffer);
            return tmp;
        }

        std::string readUntil(const std::string& delimiter) {
            while (true) {
                size_t pos = in_buffer.find(delimiter);
                if (pos != std::string::npos) {
                    size_t extract_len = pos + delimiter.length();
                    std::string result = in_buffer.substr(0, extract_len);
                    in_buffer.erase(0, extract_len); 
                    return result;
                }
                if (!recv_to_buffer()) return ""; 
            }
        }

        std::string readExactly(size_t length) {
            while (in_buffer.length() < length) {
                if (!recv_to_buffer()) return ""; 
            }
            std::string result = in_buffer.substr(0, length);
            in_buffer.erase(0, length);
            return result;
        }

        void close() {
            if (fd != -1) {
                ::close(fd);
                fd = -1;
                in_buffer.clear();
            }
        }
        
        int getFd() const { return fd; }

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

        void setHandleEvent(std::function<void(my::TcpSocket*)> _handle_event) {
            handle_event = std::move(_handle_event);
        };
    };
}

// 值传递 = 必须在函数内部构造一个新对象。至于怎么构造？既可以是拷贝构造，也可以是移动构造
// 传右值时，值传递会引发移动（只要你写了移动构造）。
// const T& 传右值也只会拷贝构造