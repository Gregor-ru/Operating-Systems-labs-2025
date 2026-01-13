#include "mcmp_queue.h"
#include <stdlib.h>

void mcmp_queue_init(MCMPQueue* q) {
    struct MCMPNode* dummy = (struct MCMPNode*)malloc(sizeof(struct MCMPNode));
    dummy->next = NULL;
    q->head = q->tail = dummy;
    pthread_mutex_init(&q->lock, NULL);
}

void mcmp_queue_destroy(MCMPQueue* q) {
    struct MCMPNode* curr = q->head;
    while (curr) {
        struct MCMPNode* next = curr->next;
        free(curr);
        curr = next;
    }
    pthread_mutex_destroy(&q->lock);
}

void mcmp_enqueue(MCMPQueue* q, int val) {
    struct MCMPNode* node = (struct MCMPNode*)malloc(sizeof(struct MCMPNode));
    node->value = val;
    node->next = NULL;

    pthread_mutex_lock(&q->lock);
    q->tail->next = node;
    q->tail = node;
    pthread_mutex_unlock(&q->lock);
}

bool mcmp_dequeue(MCMPQueue* q, int* val) {
    pthread_mutex_lock(&q->lock);
    struct MCMPNode* head = q->head;
    struct MCMPNode* first = head->next;

    if (first == NULL) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    *val = first->value;
    q->head = first;
    pthread_mutex_unlock(&q->lock);
    free(head);
    return true;
}

bool mcmp_empty(MCMPQueue* q) {
    pthread_mutex_lock(&q->lock);
    bool res = (q->head->next == NULL);
    pthread_mutex_unlock(&q->lock);
    return res;
}