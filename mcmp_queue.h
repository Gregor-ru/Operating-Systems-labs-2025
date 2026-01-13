#ifndef MCMP_QUEUE_H
#define MCMP_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

struct MCMPNode {
    int value;
    struct MCMPNode* next;
};

// Структура 8: Multiple Consumers Multiple Producers с блокировками
typedef struct {
    struct MCMPNode* head;
    struct MCMPNode* tail;
    pthread_mutex_t lock;
} MCMPQueue;

void mcmp_queue_init(MCMPQueue* q);
void mcmp_queue_destroy(MCMPQueue* q);
void mcmp_enqueue(MCMPQueue* q, int val);
bool mcmp_dequeue(MCMPQueue* q, int* val);
bool mcmp_empty(MCMPQueue* q);

#endif