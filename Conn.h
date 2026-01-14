#pragma once
#include <cstddef>

class Conn {
public:
    virtual ~Conn() = default;
    virtual bool Read(void* buf, size_t count) = 0;
    virtual bool Write(const void* buf, size_t count) = 0;
    
    virtual void OnFork(bool is_parent) = 0;
    
    virtual int GetFd() = 0;
    
    virtual void Close() {}
};

Conn* CreateConn();