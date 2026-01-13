#ifndef CONN_SOCK_H
#define CONN_SOCK_H

#include "conn_base.h"
#include <semaphore.h>
#include <string>

class ConnSock : public Conn {
private:
    int fd;
    std::string base_name;
    sem_t* sem_read;
    sem_t* sem_write;
    bool is_creator;

public:
    ConnSock(int fd, const std::string& name, bool create);
    ~ConnSock() override;
    bool Read(void *buf, size_t count) override;
    bool Write(const void *buf, size_t count) override;

    static bool CreateResources(const std::string& name);
};

#endif // CONN_SOCK_H