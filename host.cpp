#include "Conn.h"
#include "Shared.h"
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <poll.h>
#include <cstring>
#include <ctime>
#include <csignal>
#include <iomanip>

void PrintLog(const char* sender, const char* msg) {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    std::cout << "[" << std::put_time(tm_info, "%H:%M:%S") << "] "
              << "[" << sender << "]: " << msg << std::endl;
}

void ClientLogic(Conn* conn, SharedState* state, const std::string& username) {
    // 1. Сообщаем родителю, что IPC готов
    sem_post(&state->sem_client_ready);

    std::string my_name = username + "_client";
    time_t last_msg_time = time(nullptr);
    
    // Если поставить < 60, клиент никогда не "умрет".
    double msg_interval = 17.0; 

    struct pollfd fds[1];
    fds[0].fd = conn->GetFd();
    fds[0].events = POLLIN;

    while (state->is_running) {
        time_t now = time(nullptr);

        // Отправка авто-сообщения
        if (difftime(now, last_msg_time) >= msg_interval) {
            ChatMessage msg;
            msg.type = MSG_TEXT;
            msg.timestamp = now;
            strncpy(msg.username, my_name.c_str(), sizeof(msg.username)-1);
            snprintf(msg.text, sizeof(msg.text), "Я живой (авто-сообщение)");
            
            if (conn->Write(&msg, sizeof(msg))) {
                last_msg_time = now;
            } else {
                break; 
            }
        }

        int ret = poll(fds, 1, 100);
        if (ret > 0 && (fds[0].revents & POLLIN)) {
            ChatMessage in_msg;
            if (!conn->Read(&in_msg, sizeof(in_msg))) break;
            
            if (in_msg.type == MSG_EXIT) break;
            
            if (in_msg.type == MSG_TEXT) {
                std::cout << "[" << in_msg.username << "]: " << in_msg.text << std::endl;
            }
        }
    }
    conn->Close();
}

void HostLogic(Conn* conn, SharedState* state, const std::string& username, pid_t client_pid) {
    std::cout << "Ожидание подключения клиента..." << std::endl;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

    if (sem_timedwait(&state->sem_client_ready, &ts) == -1) {
        std::cerr << "Ошибка: Клиент не подключился за 5 секунд!" << std::endl;
        kill(client_pid, SIGKILL);
        return;
    }

    std::cout << "--> Клиент подключен. Чат начат (/exit для выхода)." << std::endl;
    time_t last_activity = time(nullptr);
    
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; 
    fds[0].events = POLLIN;
    fds[1].fd = conn->GetFd();
    fds[1].events = POLLIN;

    while (state->is_running) {
        time_t now = time(nullptr);

        // Проверка неактивности клиента (> 60 сек)
        if (difftime(now, last_activity) > 60.0) {
            std::cout << "\n--> Клиент неактивен более 60 сек. Отправка SIGKILL." << std::endl;
            kill(client_pid, SIGKILL);
            state->is_running = false;
            break;
        }

        int ret = poll(fds, 2, 500);
        if (ret < 0) break;

        // 1. Ввод пользователя (Хост)
        if (fds[0].revents & POLLIN) {
            std::string input;
            if (!std::getline(std::cin, input)) break;
            
            if (input == "/exit") {
                ChatMessage m; m.type = MSG_EXIT;
                conn->Write(&m, sizeof(m));
                state->is_running = false;
                break;
            }

            ChatMessage m;
            m.type = MSG_TEXT;
            m.timestamp = now;
            strncpy(m.username, username.c_str(), sizeof(m.username)-1);
            strncpy(m.text, input.c_str(), sizeof(m.text)-1);
            conn->Write(&m, sizeof(m));
            
            PrintLog("ВЫ", input.c_str());
        }

        // 2. Сообщения от клиента
        if (fds[1].revents & POLLIN) {
            ChatMessage in_msg;
            if (conn->Read(&in_msg, sizeof(in_msg))) {
                last_activity = now; 
                if (in_msg.type == MSG_TEXT) {
                    PrintLog(in_msg.username, in_msg.text);
                }
            } else {
                break;
            }
        }
    }
    
    waitpid(client_pid, NULL, 0);
    conn->Close();
    std::cout << "Работа завершена." << std::endl;
}

int main() {
    std::string user;
    std::cout << "Введите имя пользователя: ";
    if (!(std::cin >> user)) return 0;
    std::cin.ignore(); 

    void* ptr = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, 
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    SharedState* state = (SharedState*)ptr;
    
    // Инициализация семафора (shared между процессами = 1)
    sem_init(&state->sem_client_ready, 1, 0);
    state->is_running = true;

    Conn* conn = CreateConn();
    if (!conn) return 1;

    pid_t pid = fork();
    if (pid < 0) return 1;

    // Инициализация IPC
    conn->OnFork(pid > 0);

    if (pid == 0) {
        // Ребенок
        ClientLogic(conn, state, user);
        delete conn;
    } else {
        // Родитель
        HostLogic(conn, state, user, pid);
        
        sem_destroy(&state->sem_client_ready);
        munmap(ptr, sizeof(SharedState));
        delete conn;
    }

    return 0;
}