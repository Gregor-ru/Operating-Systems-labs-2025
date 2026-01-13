#ifndef CONN_BASE_H
#define CONN_BASE_H

#include <cstddef>
#include <string>

class Conn {
public:
    virtual ~Conn() = default;
    virtual bool Read(void *buf, size_t count) = 0;
    virtual bool Write(const void *buf, size_t count) = 0;

    static Conn* create(const std::string& type, const std::string& name, bool create, int fd = -1);
};

#endif // CONN_BASE_H