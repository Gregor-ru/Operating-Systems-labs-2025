#pragma once
#include <sys/types.h>
#include <semaphore.h>
#include <time.h>

enum MsgType {
    MSG_TEXT = 0,
    MSG_EXIT = 2
};

struct ChatMessage {
    int type;
    char username[32];
    char text[256];
    time_t timestamp;
};

struct SharedState {
    sem_t sem_client_ready; // Семафор готовности клиента
    bool is_running;        
};