#include "conn_mq.h"
#include <cstring>
#include <cerrno>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

ConnMq::ConnMq(const std::string& name, bool create, int fd) : mq((mqd_t)-1), mq_name(name), sem_read(SEM_FAILED), sem_write(SEM_FAILED), is_creator(create) {
    mq = mq_open(name.c_str(), O_RDWR);
    if (mq == (mqd_t)-1) {
        std::cerr << "[MQ] Ошибка открытия очереди: " << strerror(errno) << std::endl;
        return;
    }

    std::string sem_read_name = name + "_read";
    std::string sem_write_name = name + "_write";

    sem_read = sem_open(sem_read_name.c_str(), 0);
    sem_write = sem_open(sem_write_name.c_str(), 0);
    
    if (sem_read == SEM_FAILED || sem_write == SEM_FAILED) {
        std::cerr << "[MQ] Ошибка открытия семафоров" << std::endl;
    }
}

ConnMq::~ConnMq() {
    if (mq != (mqd_t)-1) mq_close(mq);
    if (is_creator) {
        mq_unlink(mq_name.c_str());
        sem_unlink((mq_name + "_read").c_str());
        sem_unlink((mq_name + "_write").c_str());
    }
    if (sem_read != SEM_FAILED) sem_close(sem_read);
    if (sem_write != SEM_FAILED) sem_close(sem_write);
}

bool ConnMq::Read(void *buf, size_t count) {
    if (mq == (mqd_t)-1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5; // Таймаут 5 сек

    if (sem_timedwait(sem_read, &ts) == -1) {
        // Если таймаут - просто возвращаем false без шума
        if (errno == ETIMEDOUT) return false;
        std::cerr << "[MQ] Ошибка sem_read: " << strerror(errno) << std::endl;
        return false;
    }

    char temp[256];
    // Используем non-blocking receive или обычный, т.к. семафор уже пропустил
    if (mq_receive(mq, temp, 256, nullptr) == -1) {
        std::cerr << "[MQ] Ошибка чтения очереди: " << strerror(errno) << std::endl;
        sem_post(sem_write);
        return false;
    }
    memcpy(buf, temp, std::min(count, (size_t)256));

    sem_post(sem_write);
    return true;
}

bool ConnMq::Write(const void *buf, size_t count) {
    if (mq == (mqd_t)-1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(sem_write, &ts) == -1) {
        if (errno == ETIMEDOUT) return false; // Таймаут записи
        std::cerr << "[MQ] Ошибка sem_write: " << strerror(errno) << std::endl;
        return false;
    }

    if (mq_send(mq, (const char*)buf, count, 0) == -1) {
        std::cerr << "[MQ] Ошибка записи: " << strerror(errno) << std::endl;
        sem_post(sem_read);
        return false;
    }

    sem_post(sem_read);
    return true;
}

bool ConnMq::CreateResources(const std::string& name) {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 256;
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666, &attr);
    if (mq != (mqd_t)-1) mq_close(mq);

    sem_t* s_read = sem_open((name + "_read").c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (s_read != SEM_FAILED) sem_close(s_read);

    sem_t* s_write = sem_open((name + "_write").c_str(), O_CREAT | O_EXCL, 0666, 1);
    if (s_write != SEM_FAILED) sem_close(s_write);

    return true;
}