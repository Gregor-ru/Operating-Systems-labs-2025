#ifndef CHAT_LOGIC_H
#define CHAT_LOGIC_H

struct ChatMessage {
    char text[200];  // фиксированный размер
    bool general;
    int timestamp;
};

struct Heartbeat {
    int code;  // 0 - ping
};

#endif // CHAT_LOGIC_H