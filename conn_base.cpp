#include "conn_base.h"
#include "conn_mq.h"
#include "conn_fifo.h"
#include "conn_sock.h"
#include <iostream>

Conn* Conn::create(const std::string& type, const std::string& name, bool create, int fd) {
    if (type == "mq")   return new ConnMq(name, create, fd);
    if (type == "fifo") return new ConnFifo(name, create, fd);
    if (type == "sock") return new ConnSock(fd, name, create);
    std::cerr << "Неизвестный тип: " << type << std::endl;
    return nullptr;
}