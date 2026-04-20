#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork(); // 影分身

    if (pid < 0) {
        std::cerr << "Fork 失败！\n";
        return 1;
    } 
    else if (pid == 0) {
        // --- 子进程区域 ---
        // 执行当前目录下的 a.out。
        // 第一个参数是路径，第二个是传给程序的 argv[0]，最后一个必须是 NULL
        execl("./p.exe", "p.exe", NULL);
        
        // 如果 execl 执行成功，代码绝对不会走到这里（因为灵魂已经换成 a.out 了）。
        // 如果走到这里，说明夺舍失败（比如找不到 a.out 文件）
        return -1; 
    } 
    else {
        // --- 父进程区域 ---
        int status;
        // 老老实实等着给子进程收尸，status 会记录子进程是怎么死的
        waitpid(pid, &status, 0); 

        // 核心判断：子进程是否异常退出？
        // WIFSIGNALED: 判断是否被信号杀死（比如段错误段异常、被 kill 掉）
        // WIFEXITED & WEXITSTATUS: 判断是否正常 return，但 return 的值不是 0
        bool killed_by_signal = WIFSIGNALED(status);
        bool returned_error = WIFEXITED(status) && (WEXITSTATUS(status) != 0);

        if (killed_by_signal || returned_error) {
            std::cout << "error\n";
        }
    }

    return 0;
}