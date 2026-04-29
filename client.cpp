#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

#include "my_TcpSocket.hpp"


int main() {
    my::TcpSocket client;
    client.connectTo("lengtiming.dpdns.org", 8080);
    
    std::string body = "666\n"; 
        
    // 严格按照 HTTP/1.1 协议组装报文
    std::string http_request = 
        "POST /api/sendmsg HTTP/1.1\r\n"  // 假设你的服务器路由是这个
        "Host: lengtiming.dpdns.org\r\n"  // 必须带 Host 字段，Cloudflare 靠这个认人
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.length()) + "\r\n"
        "Connection: close\r\n"           // 告诉 CF 发完就挂断，不然你的 read 会一直死等
        "\r\n"                            // 致命空行，区分头部和身体
        + body;                           // 塞入真正的数据

    // 发送规范的 HTTP 请求
    client.write(http_request);

    std::cout << client.readExactly(my::TcpSocket::getBodyLength(client.readUntil("\r\n\r\n"))) << '\n';
}