#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <thread>
#include <mutex>

#include "my_TcpSocket.hpp"

// 全局互斥锁，防止多线程同时写文件导致消息错乱
std::mutex file_mutex;

// ==================== 极简公共大厅前端 HTML ====================
const char* html_body = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>极简公共大厅</title>
    <style>
        body { font-family: sans-serif; text-align: center; background: #f4f4f9; margin-top: 50px; }
        .container { width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        #chatBox { display: none; }
        #history { height: 300px; overflow-y: auto; text-align: left; background: #fafafa; border: 1px solid #ddd; padding: 15px; margin-bottom: 15px; border-radius: 4px; white-space: pre-wrap;}
        input { padding: 10px; width: 60%; border: 1px solid #ccc; border-radius: 4px; }
        button { padding: 10px 20px; cursor: pointer; background: #007BFF; color: white; border: none; border-radius: 4px; font-weight: bold;}
        button:hover { background: #0056b3; }
    </style>
</head>
<body>
    <div class="container">
        <h1 style="color: #333;">💬 公共聊天大厅</h1>
        <div id="loginBox">
            <input type="text" id="username" placeholder="输入你的大名进入聊天室...">
            <button onclick="joinChat()">进入大厅</button>
        </div>
        <div id="chatBox">
            <div id="history">读取历史消息中...</div>
            <input type="text" id="msg" placeholder="说点什么..." onkeydown="if(event.keyCode==13) sendMsg()">
            <button onclick="sendMsg()">发送</button>
        </div>
    </div>
    <script>
        let myName = "";
        function joinChat() {
            let nameInput = document.getElementById("username").value.trim();
            if (!nameInput) { alert("名字不能为空！"); return; }
            myName = nameInput;
            document.getElementById("loginBox").style.display = "none";
            document.getElementById("chatBox").style.display = "block";
            fetchHistory();
            setInterval(fetchHistory, 2000);
        }
        function fetchHistory() {
            fetch('/api/getintxt')
            .then(response => response.text())
            .then(data => {
                let histDiv = document.getElementById("history");
                if (histDiv.innerText !== data) {
                    histDiv.innerText = data; 
                    histDiv.scrollTop = histDiv.scrollHeight;
                }
            });
        }
        function sendMsg() {
            let msgInput = document.getElementById("msg");
            let text = msgInput.value.trim();
            if (!text) return;
            let fullMsg = "[" + myName + "]: " + text;
            fetch('/api/sendmsg', {
                method: 'POST',
                body: fullMsg
            }).then(() => {
                msgInput.value = ""; 
                fetchHistory(); 
            });
        }
    </script>
</body>
</html>
)rawhtml";

// ==================== 业务逻辑 (线程回调) ====================

// 注意这里：参数变为按值传递（因为外面会用 std::move 把所有权转移给它）
void handle_client(my::TcpSocket client) {
    // 1. 精确获取 HTTP 头部
    std::string header = client.readUntil("\r\n\r\n");
    if (header.empty()) return; // 客户端关闭连接

    // 路由 1：获取历史聊天记录 (GET 请求)
    if (header.find("GET /api/getintxt") == 0) {
        std::string file_content;
        {
            // 加锁读文件
            std::lock_guard<std::mutex> lock(file_mutex);
            std::ifstream infile("in.txt");
            if (infile.is_open()) {
                std::string line;
                while (std::getline(infile, line)) {
                    file_content += line + "\n";
                }
            }
        }
        
        std::string http_response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(file_content.length()) + "\r\n\r\n" + 
            file_content;

        client.write(http_response); 
    } 
    
    // 路由 2：接收并保存新消息 (POST 请求)
    else if (header.find("POST /api/sendmsg") == 0) {
        // [核心改进] 从 header 中解析 Content-Length，防断包截断！
        size_t content_length = my::TcpSocket::getBodyLength(header);

        // 精确读取对应长度的 Body
        std::string body = client.readExactly(content_length);

        if (!body.empty()) {
            // 加锁写文件，防止多个人同时发消息把文件写乱
            std::lock_guard<std::mutex> lock(file_mutex);
            std::ofstream outfile("in.txt", std::ios::app);
            if (outfile.is_open()) {
                outfile << body << "\n";
            }
            std::cout << "收到新消息: " << body << "\n";
        }

        std::string http_response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        client.write(http_response);
    }
    
    // 路由 3：请求网页界面 (默认首页)
    else {
        std::string body_str = html_body;
        std::string http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(body_str.length()) + "\r\n\r\n" + 
            body_str;
            
        client.write(http_response); 
    }

    // 函数结束，局部变量 client 的生命周期结束，析构函数自动调用 close()！
}

int main() {
    try {
        my::TcpSocket server;
        server.bindAndListen(8080);
        std::cout << "🚀 聊天室服务器启动成功！正在监听 8080 端口...\n";

        while (true) {
            // 拿到新客人的连接对象
            my::TcpSocket client = server.acceptClient(); 
            
            // 【极其关键】TcpSocket 是独占的，必须用 std::move 转移给子线程！
            std::thread t(handle_client, std::move(client));
            t.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "服务器崩溃: " << e.what() << "\n";
    }
    
    return 0;
}