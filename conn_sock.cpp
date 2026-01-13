#include <fcntl.h>
#include "conn_sock.h"
#include <cstring>
#include <cerrno>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

ConnSock::ConnSock(int fd_, const std::string& name, bool create) 
    : fd(fd_), base_name(name), sem_read(SEM_FAILED), sem_write(SEM_FAILED), is_creator(create) {
    
    sem_read = sem_open((name + "_read").c_str(), 0);
    sem_write = sem_open((name + "_write").c_str(), 0);

    if (sem_read == SEM_FAILED || sem_write == SEM_FAILED) {
         std::cerr << "[SOCK] Ошибка открытия семафоров" << std::endl;
    }
}

ConnSock::~ConnSock() {
    if (fd != -1) close(fd);
    if (is_creator) {
        sem_unlink((base_name + "_read").c_str());
        sem_unlink((base_name + "_write").c_str());
    }
    if (sem_read != SEM_FAILED) sem_close(sem_read);
    if (sem_write != SEM_FAILED) sem_close(sem_write);
}

bool ConnSock::Read(void *buf, size_t count) {
    if (fd == -1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(sem_read, &ts) == -1) {
        if (errno == ETIMEDOUT) return false;
        std::cerr << "[SOCK] Ошибка sem_read: " << strerror(errno) << std::endl;
        return false;
    }

    ssize_t bytes = recv(fd, buf, count, 0);
    if (bytes == -1) {
        std::cerr << "[SOCK] Ошибка чтения: " << strerror(errno) << std::endl;
        sem_post(sem_write);
        return false;
    }

    sem_post(sem_write);
    return bytes > 0;
}

bool ConnSock::Write(const void *buf, size_t count) {
    if (fd == -1 || sem_read == SEM_FAILED || sem_write == SEM_FAILED) return false;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(sem_write, &ts) == -1) {
        if (errno == ETIMEDOUT) return false;
        std::cerr << "[SOCK] Ошибка sem_write: " << strerror(errno) << std::endl;
        return false;
    }

    ssize_t bytes = send(fd, buf, count, 0);
    if (bytes == -1) {
        std::cerr << "[SOCK] Ошибка записи: " << strerror(errno) << std::endl;
        sem_post(sem_read);
        return false;
    }

    sem_post(sem_read);
    return true;
}

bool ConnSock::CreateResources(const std::string& name) {
    sem_t* s_read = sem_open((name + "_read").c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (s_read != SEM_FAILED) sem_close(s_read);

    sem_t* s_write = sem_open((name + "_write").c_str(), O_CREAT | O_EXCL, 0666, 1);
    if (s_write != SEM_FAILED) sem_close(s_write);

    return true;
}