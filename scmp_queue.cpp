#include "scmp_queue.h"
#include <stdlib.h>

void scmp_queue_init(SCMPQueue* q) {
    struct SCMPNode* dummy = (struct SCMPNode*)malloc(sizeof(struct SCMPNode));
    dummy->next = NULL;
    q->head = q->tail = dummy;
}

void scmp_queue_destroy(SCMPQueue* q) {
    struct SCMPNode* curr = q->head;
    while (curr) {
        struct SCMPNode* next = curr->next;
        free(curr);
        curr = next;
    }
}

void scmp_enqueue(SCMPQueue* q, int val) {
    struct SCMPNode* node = (struct SCMPNode*)malloc(sizeof(struct SCMPNode));
    node->value = val;
    node->next = NULL;

    // Атомарно заменяем указатель на хвост. 
    // Возвращает старое значение хвоста.
    struct SCMPNode* prev_tail = (struct SCMPNode*)__sync_lock_test_and_set(&(q->tail), node);
    prev_tail->next = node;
}

bool scmp_dequeue(SCMPQueue* q, int* val) {
    struct SCMPNode* head = q->head;
    struct SCMPNode* first = head->next;

    if (first == NULL) return false;

    *val = first->value;
    q->head = first;
    free(head);
    return true;
}

bool scmp_empty(SCMPQueue* q) {
    return q->head->next == NULL;
}