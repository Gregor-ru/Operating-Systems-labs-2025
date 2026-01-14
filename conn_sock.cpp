#include "Conn.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <cstdlib>

class ConnSock : public Conn {
private:
    int sock_fd = -1;
    std::string socket_path;
    bool is_host = false;

public:
    void OnFork(bool is_parent) override {
        is_host = is_parent;
        pid_t p_pid = is_parent ? getpid() : getppid();
        socket_path = "/tmp/lab2_sock_" + std::to_string(p_pid);

        if (is_parent) {
            unlink(socket_path.c_str());
            int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            
            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

            bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
            listen(server_fd, 1);

            // Принимаем соединение
            sock_fd = accept(server_fd, NULL, NULL);
            close(server_fd); // Слушающий сокет больше не нужен
        } else {
            sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

            while (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                usleep(100000);
            }
        }
    }

    bool Read(void* buf, size_t count) override {
        return recv(sock_fd, buf, count, 0) > 0;
    }

    bool Write(const void* buf, size_t count) override {
        return send(sock_fd, buf, count, 0) == (ssize_t)count;
    }

    int GetFd() override { return sock_fd; }

    void Close() override {
        if (sock_fd != -1) close(sock_fd);
        if (is_host) unlink(socket_path.c_str());
    }
};

Conn* CreateConn() { return new ConnSock(); }