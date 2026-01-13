#include "conn_fifo.h"
#include <cstring>
#include <cerrno>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

ConnFifo::ConnFifo(const std::string& name, bool create, int fd) 
    : fd_read(-1), fd_write(-1), 
      fifo_name_read(name + "_read"), fifo_name_write(name + "_write"), 
      base_name(name), sem_read(SEM_FAILED), sem_write(SEM_FAILED), is_creator(create) {
    
    // ВАЖНО: Порядок открытия должен быть согласован, чтобы не было Deadlock.
    // Если create=true: открываем WriteWRONLY, потом ReadRDONLY
    // Если create=false: открываем ReadRDONLY, потом WriteWRONLY
    // Но для двух раздельных каналов (как мы сделали в host.cpp) это проще.
    // Оставим твою логику, она рабочая для FIFO.

    if (create) {
        fd_write = open(fifo_name_read.c_str(), O_WRONLY); // Ждет пока другой откроет на чтение
        fd_read = open(fifo_name_write.c_str(), O_RDONLY);
    } else {
        fd_read = open(fifo_name_read.c_str(), O_RDONLY);
        fd_write = open(fifo_name_write.c_str(), O_WRONLY);
    }

    if (fd_read == -1 || fd_write == -1) {
        std::cerr << "[FIFO] Ошибка открытия труб" << std::endl;
    }

    sem_read = sem_open((name + "_read").c_str(), 0);
    sem_write = sem_open((name + "_write").c_str(), 0);
}

ConnFifo::~ConnFifo() {
    if (fd_read != -1) close(fd_read);
    if (fd_write != -1) close(fd_write);
    if (is_creator) {
        unlink(fifo_name_read.c_str());
        unlink(fifo_name_write.c_str());
        sem_unlink((base_name + "_read").c_str());
        sem_unlink((base_name + "_write").c_str());
    }
    if (sem_read != SEM_FAILED) sem_close(sem_read);
    if (sem_write != SEM_FAILED) sem_close(sem_write);
}

bool ConnFifo::Read(void *buf, size_t count) {
    if (fd_read == -1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(sem_read, &ts) == -1) {
        if (errno == ETIMEDOUT) return false;
        std::cerr << "[FIFO] Ошибка sem_read: " << strerror(errno) << std::endl;
        return false;
    }

    ssize_t bytes = read(fd_read, buf, count);
    if (bytes == -1) {
        std::cerr << "[FIFO] Ошибка чтения: " << strerror(errno) << std::endl;
        sem_post(sem_write);
        return false;
    }

    sem_post(sem_write);
    return bytes > 0;
}

bool ConnFifo::Write(const void *buf, size_t count) {
    if (fd_write == -1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(sem_write, &ts) == -1) {
        if (errno == ETIMEDOUT) return false;
        std::cerr << "[FIFO] Ошибка sem_write: " << strerror(errno) << std::endl;
        return false;
    }

    ssize_t bytes = write(fd_write, buf, count);
    if (bytes == -1) {
        std::cerr << "[FIFO] Ошибка записи: " << strerror(errno) << std::endl;
        sem_post(sem_read);
        return false;
    }

    sem_post(sem_read);
    return true;
}

bool ConnFifo::CreateResources(const std::string& name) {
    mkfifo((name + "_read").c_str(), 0666);
    mkfifo((name + "_write").c_str(), 0666);

    sem_t* s_read = sem_open((name + "_read").c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (s_read != SEM_FAILED) sem_close(s_read);

    sem_t* s_write = sem_open((name + "_write").c_str(), O_CREAT | O_EXCL, 0666, 1);
    if (s_write != SEM_FAILED) sem_close(s_write);

    return true;
}