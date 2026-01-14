#include "Conn.h"
#include <mqueue.h>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

class ConnMQ : public Conn {
private:
    mqd_t mq_read = -1;
    mqd_t mq_write = -1;
    std::string name_read;
    std::string name_write;
    const int MSG_SIZE = 1024;

public:
    void OnFork(bool is_parent) override {
        // Формируем имена очередей на основе PID родителя
        pid_t p_pid = is_parent ? getpid() : getppid();
        std::string q1 = "/lab2_mq_1_" + std::to_string(p_pid);
        std::string q2 = "/lab2_mq_2_" + std::to_string(p_pid);

        // Родитель читает из 1, пишет в 2. Ребенок наоборот.
        name_read = is_parent ? q1 : q2;
        name_write = is_parent ? q2 : q1;

        struct mq_attr attr = {0, 10, MSG_SIZE, 0};

        if (is_parent) {
            mq_unlink(name_read.c_str());
            mq_unlink(name_write.c_str());
            // Родитель создает очереди
            mq_read = mq_open(name_read.c_str(), O_CREAT | O_RDONLY, 0644, &attr);
            mq_write = mq_open(name_write.c_str(), O_CREAT | O_WRONLY, 0644, &attr);
        } else {
            // Клиент ждет создания очередей родителем
            while (true) {
                mq_read = mq_open(name_read.c_str(), O_RDONLY);
                mq_write = mq_open(name_write.c_str(), O_WRONLY);
                if (mq_read != -1 && mq_write != -1) break;
                if (mq_read != -1) close(mq_read);
                if (mq_write != -1) close(mq_write);
                usleep(100000);
            }
        }
    }

    bool Read(void* buf, size_t count) override {
        return mq_receive(mq_read, (char*)buf, MSG_SIZE, nullptr) != -1;
    }

    bool Write(const void* buf, size_t count) override {
        return mq_send(mq_write, (const char*)buf, count, 0) != -1;
    }

    int GetFd() override { return (int)mq_read; }

    void Close() override {
        if (mq_read != -1) mq_close(mq_read);
        if (mq_write != -1) mq_close(mq_write);
    }
};

Conn* CreateConn() { return new ConnMQ(); }