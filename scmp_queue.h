#ifndef SCMP_QUEUE_H
#define SCMP_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

struct SCMPNode {
    int value;
    struct SCMPNode* next;
};

// Структура 7: Single Consumer Multiple Producers
typedef struct {
    struct SCMPNode* head;
    struct SCMPNode* tail;
} SCMPQueue;

void scmp_queue_init(SCMPQueue* q);
void scmp_queue_destroy(SCMPQueue* q);
void scmp_enqueue(SCMPQueue* q, int val);
bool scmp_dequeue(SCMPQueue* q, int* val);
bool scmp_empty(SCMPQueue* q);

#endif