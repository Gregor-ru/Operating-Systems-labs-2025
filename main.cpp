#include <iostream>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#include <iomanip>
#include <sched.h>
#include "scmp_queue.h"
#include "mcmp_queue.h"

// Глобальные данные для тестов
int* verify_array;
int num_elements;

struct ThreadArgs {
    int id;
    int count;
    int total_writers; // Добавили поле, чтобы считать шаг
    void* q;
    bool is_mcmp;
};

void* producer(void* arg) {
    ThreadArgs* data = (ThreadArgs*)arg;
    // Каждый писатель берет числа с шагом равным кол-ву писателей
    // Например, если 2 писателя:
    // Поток 0 пишет: 0, 2, 4, 6...
    // Поток 1 пишет: 1, 3, 5, 7...
    // Так мы гарантируем, что каждое число от 0 до N-1 встретится ОДИН раз.
    for (int i = 0; i < data->count; ++i) {
        int val = data->id + (i * data->total_writers);
        if (val < num_elements) {
            if (data->is_mcmp) mcmp_enqueue((MCMPQueue*)data->q, val);
            else scmp_enqueue((SCMPQueue*)data->q, val);
        }
    }
    return NULL;
}

void* consumer(void* arg) {
    ThreadArgs* data = (ThreadArgs*)arg;
    int extracted = 0;
    while (extracted < data->count) {
        int val;
        bool res = data->is_mcmp ? mcmp_dequeue((MCMPQueue*)data->q, &val)
                                 : scmp_dequeue((SCMPQueue*)data->q, &val);
        if (res) {
            // Атомарно увеличиваем счетчик в проверочном массиве
            __sync_fetch_and_add(&verify_array[val], 1);
            extracted++;
        } else {
            // Если очередь пуста, уступаем процессор (требование лабы)
            sched_yield(); 
        }
    }
    return NULL;
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void run_test(int W, int R, int N) {
    num_elements = N;
    verify_array = (int*)calloc(N, sizeof(int));
    
    // 1. Тест SCMP (Всегда 1 читатель по заданию)
    SCMPQueue q_scmp;
    scmp_queue_init(&q_scmp);
    std::cout << "\n--- Test: Writers=" << W << ", Readers=" << R << ", Elements=" << N << " ---" << std::endl;

    double start = get_time();
    pthread_t writers[W], readers[R];
    ThreadArgs w_args[W], r_args[R];

    int scmp_R = 1; // SCMP поддерживает только одного читателя
    for (int i = 0; i < W; i++) {
        w_args[i] = {i, N / W, W, &q_scmp, false};
        pthread_create(&writers[i], NULL, producer, &w_args[i]);
    }
    for (int i = 0; i < scmp_R; i++) {
        r_args[i] = {i, N / scmp_R, 0, &q_scmp, false};
        pthread_create(&readers[i], NULL, consumer, &r_args[i]);
    }
    for (int i = 0; i < W; i++) pthread_join(writers[i], NULL);
    for (int i = 0; i < scmp_R; i++) pthread_join(readers[i], NULL);
    
    double scmp_time = get_time() - start;
    
    bool ok = true;
    for(int i=0; i<N; i++) if(verify_array[i] != 1) ok = false;
    std::cout << "SCMP Status: " << (ok ? "OK" : "FAILED") << " | Time: " << std::fixed << std::setprecision(6) << scmp_time << "s" << std::endl;
    scmp_queue_destroy(&q_scmp);

    // 2. Тест MCMP
    for(int i=0; i<N; i++) verify_array[i] = 0;
    MCMPQueue q_mcmp;
    mcmp_queue_init(&q_mcmp);
    
    start = get_time();
    for (int i = 0; i < W; i++) {
        w_args[i] = {i, N / W, W, &q_mcmp, true};
        pthread_create(&writers[i], NULL, producer, &w_args[i]);
    }
    for (int i = 0; i < R; i++) {
        r_args[i] = {i, N / R, 0, &q_mcmp, true};
        pthread_create(&readers[i], NULL, consumer, &r_args[i]);
    }
    for (int i = 0; i < W; i++) pthread_join(writers[i], NULL);
    for (int i = 0; i < R; i++) pthread_join(readers[i], NULL);
    
    double mcmp_time = get_time() - start;
    
    ok = true;
    for(int i=0; i<N; i++) if(verify_array[i] != 1) ok = false;
    std::cout << "MCMP Status: " << (ok ? "OK" : "FAILED") << " | Time: " << mcmp_time << "s" << std::endl;
    mcmp_queue_destroy(&q_mcmp);

    free(verify_array);
}

int main() {
    // Сценарии из твоего лога
    run_test(1, 1, 100000);
    run_test(2, 1, 100000);
    run_test(4, 2, 100000); // Здесь для SCMP всё равно будет запущен 1 ридер
    return 0;
}