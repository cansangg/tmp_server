#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

#include "my_TcpSocket.hpp"


int main() {
    // 1. 组装标准 OpenAI 格式的 JSON
    std::string json_body = R"({
        "model": "qwen3.6-plus-2026-04-02",
        "messages": [
            {
                "role": "user",
                "content": "想手搓ui框架的输入和交互部分不想在乎绘制部分，那绘制部分用什么库函数来简易代替，c++"
            }
        ]
    })";
    
    // 2. 组装 HTTP 报文
    std::string http_request = 
        "POST /compatible-mode/v1/chat/completions HTTP/1.1\r\n" // <--- 注意这里改了！
        "Host: dashscope.aliyuncs.com\r\n"
        "Authorization: Bearer sk-31e586b0ac024980a0d30326ae320154\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_body.length()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + json_body;

    // 3. 用你的封装类连接本地代理！
    my::TcpSocket client;
    client.connectTo("127.0.0.1", 8080); // 连本地代理！
    client.write(http_request);

    // 4. 完美读取阿里大模型的回复
    std::string header = client.readUntil("\r\n\r\n");
    
    // 【关键调试代码】：把服务器真实的响应头打印出来！
    std::cout << "【服务器响应头】:\n" << header << "\n====================\n";

    size_t len = my::TcpSocket::getBodyLength(header);
    std::cout << "解析出的 Content-Length 是: " << len << " 字节\n";

    std::string response = client.readExactly(len);
    std::cout << "【服务器响应体】:\n" << response << std::endl;
}