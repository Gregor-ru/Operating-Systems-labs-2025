#ifndef CONN_H
#define CONN_H

#include <cstddef>

class Conn {
public:
    virtual ~Conn() = default;
    virtual bool write(const void* buf, size_t size) = 0;
    virtual bool read(void* buf, size_t size, int timeout_sec) = 0;
};

#endif
