# Server.h - 轻量级 C++ 网络通信库

**Language:** C++ | **Platform:** Windows

一个基于 Windows Socket (Winsock2) 的简单、轻量级 C++ 网络库，支持服务端/客户端模式、文件操作及非阻塞连接。

---



### 消息收发

| 函数名 | 参数 | 功能 |
| :--- | :--- | :--- |
| `push_msg` | `SERVER_MSG msg` | 发送消息 |
| `get_msg` | `SERVER_MSG& out` | 接收消息，成功返回 true |

### 文件操作

| 函数名 | 参数 | 功能 |
| :--- | :--- | :--- |
| `get_text` | `const std::string& relative, int line` | 读取指定行内容 |
| `mkfile` | `const std::string& relative` | 创建空文件 |
| `mkdir` | `const std::string& relative` | 创建目录 |
| `file_here` | `const std::string& relative` | 检查文件是否存在 |
| `rdfile` | `const std::string& relative, int line, const std::string& new_content` | 替换指定行内容 |
| `text_push` | `const std::string& relative, const std::string& str` | 追加内容到文件 |
| `file_to_string` | `const std::string& relative` | 读取整个文件 |
| `string_to_file` | `const std::string& relative, const std::string& content` | 写入整个文件 |

### 服务端控制

| 函数名 | 参数 | 功能 |
| :--- | :--- | :--- |
| `start_server` | `const std::string& root, const std::string& name, int port` | 启动服务端 |
| `stop_server` | `const std::string& name` | 停止服务端 |

### 客户端连接

| 函数名 | 参数 | 功能 |
| :--- | :--- | :--- |
| `client_connect` | `const std::string& ip, int port` | 连接服务端（5秒超时） |
| `client_close` | 无 | 关闭连接 |
| `turn` | `const std::string& ip, int port` | 切换连接 |
| `check` | `const std::string& ip, int port` | 检查服务端是否可连（3秒超时） |

### 工具函数

| 函数名 | 参数 | 功能 |
| :--- | :--- | :--- |
| `split_str` | `const std::string& s, const std::string& delim` | 分割字符串 |
| `to_num` | `const std::string& s` | 字符串转整数 |
| `get_ipv4` | 无 | 获取公网 IPv4 |
| `get_ipv6` | 无 | 获取公网 IPv6 |
| `make_path` | `const std::string& relative` | 拼接根目录路径 |

---

## 数据结构

### SERVER_MSG

| 成员 | 类型 | 说明 |
| :--- | :--- | :--- |
| `s_ip` | `std::string` | IP 地址 |
| `s_msg` | `std::string` | 消息内容 |
| `s_port` | `int` | 端口号 |

### ClientInfo

| 成员 | 类型 | 说明 |
| :--- | :--- | :--- |
| `fd` | `SOCKET` | Socket 句柄 |
| `ip` | `std::string` | IP 地址 |
| `port` | `int` | 端口号 |

---

## 示例代码

### 客户端

```cpp
#include <iostream>
#include <string>
#include "server.h"
using namespace std;

int main() {
    int pt;
    string ip;

    cout << "服务器IP: ";
    cin >> ip;
    cout << "端口: ";
    cin >> pt;

    if (!client_connect(ip, pt)) {
        cout << "连接失败！请检查服务器是否运行" << endl;
        system("pause");
        return 1;
    }

    cout << "\n连接成功！输入消息发送，stop 退出，turn 重连\n";

    string msgs;
    while (true) {
        cout << "> ";
        cin >> msgs;

        if (msgs == "stop") {
            break;
        }

        if (msgs == "turn") {
            turn(ip, pt); 
            cout << "已重连！\n";
            continue;
        }

        SERVER_MSG msg;
        msg.s_ip = ip; 
        msg.s_msg = "[FROM_CLIENT]" + msgs;
        msg.s_port = pt;
        push_msg(msg);
    }

    client_close();
    cout << "已退出" << endl;
    return 0;
}
```
# 服务端
```cpp
#include "server.h"
#include <iostream>
#include <thread>
#include <vector>
using namespace std;

bool gr = true;

void run() {
    while (gr) {
        SERVER_MSG msgs;
        if (get_msg(msgs)) {
            cout << "\n[收到消息] " << msgs.s_ip << ":" << msgs.s_port << " > " << msgs.s_msg << endl;
            cout << "> ";  // 重新显示提示符

            // 处理服务器端命令
            if (msgs.s_msg == "cls") {
                system("cls");
                cout << "屏幕已清屏" << endl;
            }
            else if (msgs.s_msg == "server_stop") {
                cout << "收到停止命令，服务器即将关闭..." << endl;
                gr = false;
                break;
            }

            // 处理创建文件命令: mkf 文件路径 文件内容
            vector<string> cmd = split_str(msgs.s_msg, " ");
            if (cmd.size() >= 3 && cmd[0] == "mkf") {
                string path = cmd[1];
                string content;
                for (size_t i = 2; i < cmd.size(); i++) {
                    if (i > 2) content += " ";
                    content += cmd[i];
                }
                if (string_to_file(path, content)) {
                    cout << "文件创建成功: " << path << endl;
                }
                else {
                    cout << "文件创建失败: " << path << endl;
                }
            }
        }
        Sleep(10);
    }
}

int main() {
    cout << "========================================" << endl;
    cout << "服务器公网IPv4: " << get_ipv4() << endl;
    cout << "========================================" << endl;

    string rp = "C:\\tt.server_1";
    string sn = "1";
    int pt;

    cout << "请输入监听端口: ";
    cin >> pt;

    bool ok = start_server(rp, sn, pt);
    if (ok) {
        cout << "✓服务器已启动，端口: " << pt << endl;
        cout << "命令: stop - 关闭服务器" << endl;
        cout << "========================================" << endl;
    }
    else {
        cout << " 启动失败！端口可能被占用" << endl;
        system("pause");
        return 1;
    }

    thread run1(run);

    string cmd;
    while (true) {
        cout << "> ";
        cin >> cmd;
        if (cmd == "stop") {
            gr = false;
            break;
        }
        else {
            cout << "未知命令，可用命令: stop" << endl;
        }
    }

    run1.join();
    stop_server(sn);
    cout << "服务器已完全关闭" << endl;
    return 0;
}
```



