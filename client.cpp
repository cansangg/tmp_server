#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>      // 【新增】用于域名解析的头文件
#include <cstring>

using namespace std;

// ==================== 你的私人 Socket 封装库 ====================
int get_tcp_fd() { return socket(AF_INET, SOCK_STREAM, 0); }

void connect_to_server(int my_fd, const char* ip, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    connect(my_fd, (struct sockaddr*)&addr, sizeof(addr));
}

// 【新增工具】把 "wttr.in" 这样的域名翻译成 "5.9.243.187" 这样的 IP
string domain_to_ip(const char* domain) {
    struct hostent* host = gethostbyname(domain); // 呼叫系统的 DNS 服务
    if (host == nullptr) return "";
    return inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
}
// ================================================================

int main() {
    int my_fd = get_tcp_fd(); // 拿个自己的管道
    
    // 1. 域名解析：拿到天气 API 的真实 IP
    const char* api_domain = "lengtiming.dpdns.org"; // "wttr.in";
    string ip = domain_to_ip(api_domain);
    cout << ">> 成功解析 " << api_domain << " 的 IP 地址: " << ip << endl;

    // 2. 怼到服务器的 80 端口大门上 (HTTP 默认端口)
    connect_to_server(my_fd, ip.c_str(), 8080); 
    cout << ">> 成功连接到服务器！" << endl;

    /*// 3. 顺着管道扔请求：必须符合 HTTP 协议格式！
    // 我们请求 "/Beijing?format=3"，意思是要北京的天气，且只要一行纯文本格式
    const char* http_request = 
        "GET /Beijing?format=3 HTTP/1.1\r\n"
        "Host: wttr.in\r\n"
        "Connection: close\r\n"  // 告诉服务器：发完天气预报你就主动挂断吧
        "\r\n";                  // HTTP协议规定必须有空行结尾

    write(my_fd, http_request, strlen(http_request));
    cout << ">> 已发送天气查询请求，等待回应...\n" << endl;
    
    // 4. 顺着管道收回应 (互联网数据可能有延迟，要用 while 循环一直读，直到读完)
    char buf[1024];
    int bytes_read;
    
    cout << "================ 天气情报 ================" << endl;
    // read 如果返回 0，说明服务器发完数据并且主动断开了连接 (因为我们写了 Connection: close)
    while ((bytes_read = read(my_fd, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes_read] = '\0'; // 加上字符串结尾符
        cout << buf;            // 打印收到的数据
    }
    cout << "\n==========================================" << endl;

    close(my_fd); // 扔掉管道*/

    // 使用 R"()" 原始字符串语法，避免写一堆丑陋的 \r\n
    // 注意：HTTP 头部的换行必须是 \r\n
    string s = 
        "POST / HTTP/1.1\r\n"
        "Host: lengtiming.dpdns.org\r\n"
        "Content-Length: 3\r\n"
        //"Connection: close\r\n" // 加上这个，Cloudflare 回复完就会主动帮你断开 TCP，防止卡死
        "\r\n"
        "666";

    write(my_fd, s.c_str(), s.size());
    char buf[1024];
    int bytes_read;
    while (bytes_read = read(my_fd, buf, 1023)) {
        buf[bytes_read] = '\0';
        cout << buf;
    }
    //while (1);

    return 0;
}