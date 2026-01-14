#include "Conn.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cerrno>

class ConnFifo : public Conn {
private:
    int fd_read = -1;
    int fd_write = -1;
    std::string path_read;
    std::string path_write;
    bool is_host = false;

public:
    void OnFork(bool is_parent) override {
        is_host = is_parent;
        pid_t p_pid = is_parent ? getpid() : getppid();
        
        std::string f1 = "/tmp/lab2_fifo_1_" + std::to_string(p_pid);
        std::string f2 = "/tmp/lab2_fifo_2_" + std::to_string(p_pid);

        path_read = is_parent ? f1 : f2;
        path_write = is_parent ? f2 : f1;

        if (is_parent) {
            // 1. Удаляем старые и создаем новые
            unlink(path_read.c_str());
            unlink(path_write.c_str());
            
            if (mkfifo(path_read.c_str(), 0666) == -1) { perror("mkfifo 1"); exit(1); }
            if (mkfifo(path_write.c_str(), 0666) == -1) { perror("mkfifo 2"); exit(1); }

            // 2. Открываем каналы
            fd_read = open(path_read.c_str(), O_RDWR | O_NONBLOCK); 
            if (fd_read < 0) { perror("Parent open read"); exit(1); }

            fd_write = open(path_write.c_str(), O_RDWR);
            if (fd_write < 0) { perror("Parent open write"); exit(1); }

        } else {
            // Ждем, пока родитель создаст FIFO файлы
            int retries = 0;
            while (true) {
                fd_write = open(path_write.c_str(), O_RDWR);
                
                if (fd_write >= 0) {
                    fd_read = open(path_read.c_str(), O_RDWR | O_NONBLOCK);
                    if (fd_read >= 0) break; 
                    
                    close(fd_write); 
                }
                
                usleep(100000);
                retries++;
                if (retries > 50) {
                    std::cerr << "Timeout waiting for host pipes creation" << std::endl;
                    exit(1);
                }
            }
        }
    }

    bool Read(void* buf, size_t count) override {
        ssize_t res = read(fd_read, buf, count);
        if (res > 0) return true;
        // Если res == -1 и EAGAIN, значит данных пока нет (из-за O_NONBLOCK), но канал жив.
        if (res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false; 
        return false; 
    }

    bool Write(const void* buf, size_t count) override {
        size_t total = 0;
        const char* ptr = (const char*)buf;
        while (total < count) {
            ssize_t written = write(fd_write, ptr + total, count - total);
            if (written < 0) return false;
            total += written;
        }
        return true;
    }

    int GetFd() override { return fd_read; }

    void Close() override {
        if (fd_read != -1) close(fd_read);
        if (fd_write != -1) close(fd_write);
        if (is_host) {
            unlink(path_read.c_str());
            unlink(path_write.c_str());
        }
    }
};

Conn* CreateConn() { return new ConnFifo(); }