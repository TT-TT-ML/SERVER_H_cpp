#ifndef SERVER_H
#define SERVER_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <vector>
#include <cstring>
#include <climits>
#include <fstream>
#include <filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
namespace fs = std::filesystem;

// 消息结构
struct SERVER_MSG {
    std::string s_ip;
    std::string s_msg;
    int s_port;
};

// 客户端信息
struct ClientInfo {
    SOCKET fd;
    std::string ip;
    int port;
};

// 全局变量
inline bool g_server_running = false;
inline SOCKET g_client_socket = INVALID_SOCKET;
inline std::vector<ClientInfo> g_clients;
inline std::mutex g_cli_mutex;
inline std::queue<SERVER_MSG> g_msg_queue;
inline std::mutex g_msg_mutex;
inline std::unordered_map<std::string, SOCKET> g_servers;
inline std::unordered_map<std::string, std::thread> g_server_threads;
inline bool g_wsa_init = false;
inline std::string g_root_path;
inline std::string g_default_html;
inline bool g_use_php = false;

//=============================================================================
// 路径工具
//=============================================================================
inline std::string make_path(const std::string& relative) {
    if (!g_root_path.empty() && !fs::exists(g_root_path)) {
        fs::create_directories(g_root_path);
    }
    if (g_root_path.empty()) return relative;
    return g_root_path + "\\" + relative;
}

//=============================================================================
// 工具函数
//=============================================================================
inline std::vector<std::string> split_str(const std::string& s, const std::string& delim) {
    std::vector<std::string> res;
    if (s.empty()) return res;
    size_t pos = 0;
    while (true) {
        size_t next = s.find(delim, pos);
        if (next == std::string::npos) {
            res.push_back(s.substr(pos));
            break;
        }
        res.push_back(s.substr(pos, next - pos));
        pos = next + delim.size();
    }
    return res;
}

inline int to_num(const std::string& s) {
    if (s.empty()) return INT_MIN;
    for (char c : s) {
        if (!isdigit((unsigned char)c)) return INT_MIN;
    }
    try {
        return std::stoi(s);
    }
    catch (...) {
        return INT_MIN;
    }
}

//=============================================================================
// 文件操作函数
//=============================================================================
inline std::string get_text(const std::string& relative, int line) {
    if (line < 1) return "";
    try {
        std::ifstream f(make_path(relative));
        if (!f) return "";
        std::string s;
        int cur = 0;
        while (getline(f, s)) if (++cur == line) return s;
    }
    catch (...) {}
    return "";
}

inline bool mkfile(const std::string& relative) {
    if (relative.empty()) return false;
    try {
        std::ofstream f(make_path(relative));
        return f.is_open();
    }
    catch (...) { return false; }
}

inline bool mkdir(const std::string& relative) {
    if (relative.empty()) return false;
    try {
        return fs::create_directories(make_path(relative));
    }
    catch (...) { return false; }
}

inline bool file_here(const std::string& relative) {
    if (relative.empty()) return false;
    try {
        return fs::exists(make_path(relative));
    }
    catch (...) { return false; }
}

inline bool rdfile(const std::string& relative, int line, const std::string& new_content) {
    if (line < 1 || relative.empty()) return false;
    try {
        std::string full = make_path(relative);
        std::ifstream fin(full);
        std::vector<std::string> lines;
        if (fin.is_open()) {
            std::string s;
            while (getline(fin, s)) lines.push_back(s);
            fin.close();
        }
        while (lines.size() < (size_t)line) lines.push_back("");
        lines[line - 1] = new_content;
        std::ofstream fout(full);
        if (!fout) return false;
        for (auto& l : lines) fout << l << "\n";
        return true;
    }
    catch (...) { return false; }
}

inline bool text_push(const std::string& relative, const std::string& str) {
    if (relative.empty()) return false;
    try {
        std::ofstream f(make_path(relative), std::ios::app | std::ios::binary);
        if (!f) return false;
        f << str;
        return true;
    }
    catch (...) { return false; }
}

inline std::string file_to_string(const std::string& relative) {
    if (relative.empty()) return "";
    try {
        std::ifstream f(make_path(relative), std::ios::binary);
        if (!f) return "";
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }
    catch (...) {
        return "";
    }
}

inline bool string_to_file(const std::string& relative, const std::string& content) {
    if (relative.empty()) return false;
    try {
        std::ofstream f(make_path(relative), std::ios::binary);
        if (!f) return false;
        f.write(content.data(), content.size());
        return true;
    }
    catch (...) { return false; }
}


//=============================================================================
// 获取IP
//=============================================================================
inline std::string get_ipv4() {
    FILE* pipe = _popen("curl -4 ip.sb 2>nul", "r");
    if (!pipe) return "127.0.0.1";

    char buf[128] = { 0 };
    fread(buf, 1, 127, pipe);
    _pclose(pipe);

    std::string ip = buf;
    while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r'))
        ip.pop_back();

    return ip.empty() ? "127.0.0.1" : ip;
}

inline std::string get_ipv6() {
    FILE* pipe = _popen("curl -6 ip.sb 2>nul", "r");
    if (!pipe) return "NULL";

    char buf[128] = { 0 };
    fread(buf, 1, 127, pipe);
    _pclose(pipe);

    std::string ip = buf;
    while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r'))
        ip.pop_back();


}

//=============================================================================
// 消息收发
//=============================================================================
inline void push_msg(SERVER_MSG msg) {
    if (g_client_socket != INVALID_SOCKET) {
        send(g_client_socket, msg.s_msg.c_str(), (int)msg.s_msg.size(), 0);
        return;
    }
    std::lock_guard<std::mutex> lock(g_cli_mutex);
    for (auto& c : g_clients) {
        if (c.ip == msg.s_ip && c.port == msg.s_port) {
            send(c.fd, msg.s_msg.c_str(), (int)msg.s_msg.size(), 0);
        }
    }
}

inline bool get_msg(SERVER_MSG& out) {
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    if (g_msg_queue.empty()) return false;
    out = g_msg_queue.front();
    g_msg_queue.pop();
    return true;
}

//=============================================================================
// 客户端连接（非阻塞 + 超时，不会卡死）
//=============================================================================

inline void turn(const std::string& ip, int port);

inline bool client_connect(const std::string& ip, int port) {
    // 初始化 Winsock
    if (!g_wsa_init) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) {
            return false;
        }
        g_wsa_init = true;
    }

    // 先关闭已有连接
    if (g_client_socket != INVALID_SOCKET) {
        closesocket(g_client_socket);
        g_client_socket = INVALID_SOCKET;
    }

    // 创建 socket
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        return false;
    }

    // 设置非阻塞模式
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    // 设置地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        closesocket(s);
        return false;
    }

    // 发起非阻塞连接
    connect(s, (sockaddr*)&addr, sizeof(addr));

    // 使用 select 等待连接完成，设置 5 秒超时
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    timeval tv{ 5, 0 };  // 5秒超时

    int ret = select(0, NULL, &fds, NULL, &tv);

    if (ret <= 0) {
        if (ret == 0) {
            std::cerr << "\033[31mtime out!" << "\033[0m" << std::endl;
        }
        else {
            std::cerr << "\033[31merro: " << WSAGetLastError() << "\033[0m" << std::endl;
        }
        closesocket(s);
        return false;
    }

    // 检查连接是否真正成功
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (error != 0) {
        std::cerr << "\033[31merro: " << error << "\033[0m" << std::endl;
        closesocket(s);
        return false;
    }

    // 切换回阻塞模式
    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);

    g_client_socket = s;

    // 启动接收线程
    std::thread([s, ip, port]() {
        char buf[4096];
        while (true) {
            int n = recv(s, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                std::cerr << "\033[31mNOT SERVER!!!\033[0m" << std::endl;
                break;
            }
            buf[n] = 0;
            std::lock_guard<std::mutex> m(g_msg_mutex);
            g_msg_queue.push({ ip, buf, port });
        }
        closesocket(s);
        if (g_client_socket == s) {
            g_client_socket = INVALID_SOCKET;
        }
        }).detach();

    return true;
}

inline void client_close() {
    if (g_client_socket != INVALID_SOCKET) {
        closesocket(g_client_socket);
        g_client_socket = INVALID_SOCKET;
    }
}

inline void turn(const std::string& ip, int port) {
    client_close();
    Sleep(500);
    client_connect(ip, port);
}

inline bool check(const std::string& ip, int port) {
    SOCKET test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock == INVALID_SOCKET) return false;

    u_long mode = 1;
    ioctlsocket(test_sock, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    connect(test_sock, (sockaddr*)&addr, sizeof(addr));

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(test_sock, &fds);
    timeval tv{ 3, 0 };
    int ret = select(0, NULL, &fds, NULL, &tv);

    bool connected = false;
    if (ret > 0) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(test_sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
        connected = (error == 0);
    }

    closesocket(test_sock);
    return connected;
}

//=============================================================================
// 服务端
//=============================================================================
inline void client_recv_thread(SOCKET fd, std::string ip, int port) {
    char buf[4096];
    while (true) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = 0;
        std::lock_guard<std::mutex> m(g_msg_mutex);
        g_msg_queue.push({ ip, buf, port });
    }
    closesocket(fd);
}

inline void server_loop(SOCKET listen_fd) {
    g_server_running = true;
    while (g_server_running) {
        sockaddr_in addr{};
        int len = sizeof(addr);
        SOCKET c = accept(listen_fd, (sockaddr*)&addr, &len);
        if (c == INVALID_SOCKET) break;

        char cip[46];
        inet_ntop(AF_INET, &addr.sin_addr, cip, 46);
        int cport = ntohs(addr.sin_port);

        {
            std::lock_guard<std::mutex> m(g_cli_mutex);
            g_clients.push_back({ c, cip, cport });
        }
        std::thread(client_recv_thread, c, cip, cport).detach();
    }
}

inline bool start_server(const std::string& root, const std::string& name, int port) {
    g_root_path = root;
    if (!g_wsa_init) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); g_wsa_init = true; }

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind 失败，端口 " << port << " 可能被占用" << std::endl;
        closesocket(s);
        return false;
    }
    if (listen(s, 10) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }
    g_servers[name] = s;
    g_server_threads[name] = std::thread(server_loop, s);
    return true;
}

inline void stop_server(const std::string& name) {
    g_server_running = false;
    if (g_servers.count(name)) {
        closesocket(g_servers[name]);
        g_servers.erase(name);
    }
    if (g_server_threads.count(name)) {
        if (g_server_threads[name].joinable())
            g_server_threads[name].join();
        g_server_threads.erase(name);
    }
}

#endif