#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <string>
#include <signal.h> // 引入 signal，防止 SIGPIPE 杀掉程序

using namespace std;

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

int main() {
    // 忽略 SIGPIPE 信号，防止客户端突然断开导致 C++ 崩溃
    signal(SIGPIPE, SIG_IGN); 

    int listen_fd = get_tcp_fd();
    bind_port(listen_fd, 8080);
    start_listening(listen_fd);
    printf("🚀 图文聊天室启动成功！正在监听 8080 端口...\n");

    const char* html_body = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>图文公共大厅</title>
    <style>
        body { font-family: sans-serif; text-align: center; background: #f4f4f9; margin-top: 20px; }
        .container { width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        #chatBox { display: none; }
        #history { height: 400px; overflow-y: auto; text-align: left; background: #fafafa; border: 1px solid #ddd; padding: 15px; margin-bottom: 15px; border-radius: 4px; white-space: pre-wrap; word-wrap: break-word;}
        .controls { display: flex; gap: 10px; }
        input[type="text"] { flex: 1; padding: 10px; border: 1px solid #ccc; border-radius: 4px; }
        button { padding: 10px 15px; cursor: pointer; background: #007BFF; color: white; border: none; border-radius: 4px; }
        /* 隐藏原生极丑的上传按钮，用自定义按钮代替 */
        input[type="file"] { display: none; } 
        .upload-btn { background: #28a745; }
    </style>
</head>
<body>
    <div class="container">
        <h1 style="color: #333;">🖼️ 图文大厅</h1>
        <div id="loginBox">
            <input type="text" id="username" placeholder="输入大名...">
            <button onclick="joinChat()">进入</button>
        </div>
        <div id="chatBox">
            <div id="history">读取历史消息中...</div>
            <div class="controls">
                <input type="text" id="msg" placeholder="发消息..." onkeydown="if(event.keyCode==13) sendText()">
                <button onclick="sendText()">发送文本</button>
                <label class="upload-btn" style="padding: 10px 15px; cursor: pointer; color: white; border-radius: 4px;">
                    发图片 <input type="file" id="imgInput" accept="image/*" onchange="sendImage(this)">
                </label>
            </div>
        </div>
    </div>

    <script>
        let myName = "";
        
        function joinChat() {
            let nameInput = document.getElementById("username").value.trim();
            if (!nameInput) return;
            myName = nameInput;
            document.getElementById("loginBox").style.display = "none";
            document.getElementById("chatBox").style.display = "block";
            fetchHistory();
            setInterval(fetchHistory, 2000);
        }

        // 【优化点 1】增加 forceScroll 参数，默认不强制
        function fetchHistory(forceScroll = false) {
            fetch('/api/getintxt')
            .then(res => res.text())
            .then(data => {
                let histDiv = document.getElementById("history");
                
                if (histDiv.innerHTML !== data) {
                    // 【核心算法】在替换内容前，判断用户是否正停留在最底部
                    // scrollHeight 是总高度，scrollTop 是卷去的高度，clientHeight 是可视高度
                    // 允许 50 像素的误差，如果在这个范围内，认为用户在看最新消息
                    let isNearBottom = (histDiv.scrollHeight - histDiv.scrollTop - histDiv.clientHeight) < 50;

                    // 替换新内容
                    histDiv.innerHTML = data; 
                    
                    // 如果用户本来就在最下面，或者代码强制要求滚到底（比如自己刚发完消息），才往下滚
                    if (isNearBottom || forceScroll) {
                        histDiv.scrollTop = histDiv.scrollHeight; 
                    }
                }
            });
        }

        function sendMsgToServer(fullMsg) {
            fetch('/api/sendmsg', { method: 'POST', body: fullMsg })
            .then(() => {
                // 【优化点 2】自己发完消息后，传入 true，强制把屏幕拉回最下面
                fetchHistory(true); 
            });
        }

        function sendText() {
            let msgInput = document.getElementById("msg");
            if (!msgInput.value.trim()) return;
            let fullMsg = "<b>[" + myName + "]:</b> " + msgInput.value.trim() + "<br><br>";
            sendMsgToServer(fullMsg);
            msgInput.value = ""; 
        }

        function sendImage(fileInput) {
            let file = fileInput.files[0];
            if (!file) return;
            let reader = new FileReader();
            reader.onload = function(e) {
                let img = new Image();
                img.onload = function() {
                    let canvas = document.createElement('canvas');
                    let scale = Math.min(300 / img.width, 1);
                    canvas.width = img.width * scale;
                    canvas.height = img.height * scale;
                    let ctx = canvas.getContext('2d');
                    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
                    let base64Data = canvas.toDataURL('image/jpeg', 0.6);
                    
                    let fullMsg = "<b>[" + myName + "] 发送了图片:</b><br>";
                    fullMsg += "<img src='" + base64Data + "' style='border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.2); margin-top: 5px;'><br><br>";
                    
                    sendMsgToServer(fullMsg);
                    fileInput.value = ""; 
                };
                img.src = e.target.result;
            };
            reader.readAsDataURL(file);
        }
    </script>
</body>
</html>
)rawhtml";

    while (true) {
        int chat_fd = wait_for_new_client(listen_fd); 

        // ==========================================================
        // 【核心大升级：高鲁棒性的 HTTP 报文读取器】
        // ==========================================================
        string request_data = "";
        char buf[4096];
        int bytes_read;

        // 1. 先读第一波数据（通常包含完整的 HTTP Header 和一点点 Body）
        bytes_read = read(chat_fd, buf, sizeof(buf) - 1);
        if (bytes_read <= 0) { close(chat_fd); continue; }
        request_data.append(buf, bytes_read);

        // 2. 如果是 POST 请求，说明可能是发图片这种“大数据”，我们必须读完 Content-Length
        if (request_data.find("POST") == 0) {
            size_t cl_pos = request_data.find("Content-Length: ");
            if (cl_pos != string::npos) {
                // 提取请求体应该有的总长度
                int content_length = stoi(request_data.substr(cl_pos + 16));
                
                // 找到请求头和请求体的分界线 \r\n\r\n
                size_t header_end = request_data.find("\r\n\r\n");
                if (header_end != string::npos) {
                    int current_body_len = request_data.length() - (header_end + 4);
                    
                    // 3. 疯狂读取剩余数据，直到真实收到的 Body 长度达到 Content-Length
                    while (current_body_len < content_length) {
                        bytes_read = read(chat_fd, buf, sizeof(buf));
                        if (bytes_read <= 0) break; // 防止对面突然断开导致死循环
                        request_data.append(buf, bytes_read);
                        current_body_len += bytes_read;
                    }
                }
            }
        }

        // ==========================================================
        // 极简路由处理 (逻辑和之前类似，但现在操作的是完整的 request_data)
        // ==========================================================
        if (request_data.find("GET /api/getintxt") == 0) {
            ifstream infile("in.txt");
            string file_content;
            if (infile.is_open()) {
                string line;
                while (getline(infile, line)) { file_content += line + "\n"; }
                infile.close();
            }
            string http_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: " + to_string(file_content.length()) + "\r\n\r\n" + file_content;
            write(chat_fd, http_response.c_str(), http_response.length()); 
        } 
        else if (request_data.find("POST /api/sendmsg") == 0) {
            size_t body_start = request_data.find("\r\n\r\n");
            if (body_start != string::npos) {
                string body = request_data.substr(body_start + 4);
                ofstream outfile("in.txt", ios::app);
                if (outfile.is_open()) {
                    outfile << body; // 直接写入带 img 标签的 HTML
                    outfile.close();
                    printf(">> 收到并保存新消息 (大小: %zu 字节)\n", body.length());
                }
            }
            string http_response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
            write(chat_fd, http_response.c_str(), http_response.length());
        }
        else {
            char http_response[8192];
            sprintf(http_response, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\n\r\n%s", strlen(html_body), html_body);
            write(chat_fd, http_response, strlen(http_response)); 
        }

        close(chat_fd); 
    }
    
    return 0;
}