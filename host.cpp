#include "conn_base.h"
#include "conn_mq.h"
#include "conn_fifo.h"
#include "conn_sock.h"
#include "chat_logic.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <sys/wait.h>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <sys/socket.h>
#include <mutex>

std::mutex cout_mutex;
std::atomic<bool> running(true);
time_t last_receive = 0;

void safe_cout(const std::string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << msg << std::endl;
}

// Поток ввода: читает консоль и пишет в write_conn
void input_thread(Conn* write_conn, bool is_host) {
    while (running) {
        std::string input;
        // Мы не выводим "Введи сообщение", чтобы не ломать вывод при гонке потоков ввода
        if (!std::getline(std::cin, input)) {
             break; // EOF
        }

        if (input == "exit") {
            running = false;
            break;
        }

        ChatMessage msg{};
        strncpy(msg.text, input.c_str(), 199);
        msg.text[199] = '\0';
        msg.general = true;
        msg.timestamp = static_cast<int>(time(nullptr));

        // Пытаемся отправить
        if (!write_conn->Write(&msg, sizeof(msg))) {
            safe_cout("[" + std::string(is_host ? "HOST" : "CLIENT") + "] Ошибка отправки (таймаут или сбой)");
            // Не завершаем программу сразу, возможно канал восстановится
        } else {
            // Локальное подтверждение не обязательно, так как мы видим свой ввод
            // safe_cout("-> Отправлено"); 
        }
    }
}

// Поток приема: читает из read_conn
void receive_thread(Conn* read_conn, bool is_host) {
    while (running) {
        ChatMessage msg{};
        // Читаем с таймаутом 5 сек. Если таймаут - вернется false.
        if (!read_conn->Read(&msg, sizeof(msg))) {
            // Если вернулось false, это может быть таймаут.
            // Мы просто идем на следующий круг цикла, не убивая программу.
            continue;
        }

        safe_cout("[" + std::string(is_host ? "CLIENT" : "HOST") + "] Получено: " + std::string(msg.text));
        
        if (is_host) {
            last_receive = time(nullptr);
        }
    }
}

void heartbeat_thread(Conn* heart_conn) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Чаще шлем heartbeat для теста
        Heartbeat hb = {0};
        if (!heart_conn->Write(&hb, sizeof(hb))) {
            // Если heartbeat не ушел - не страшно, попробуем позже
            // safe_cout("[CLIENT] Heartbeat skip");
        } 
    }
}

void heart_receive_thread(Conn* heart_conn) {
    while (running) {
        Heartbeat hb;
        if (heart_conn->Read(&hb, sizeof(hb))) {
            last_receive = time(nullptr);
            // safe_cout("DEBUG: Heartbeat получен");
        }
    }
}

void monitor_thread(pid_t child_pid) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        time_t now = time(nullptr);
        // safe_cout("DEBUG: Мониторинг | разница = " + std::to_string(now - last_receive) + " сек");
        
        if (now - last_receive > 60) {
            safe_cout("[HOST] Клиент неактивен >60 сек → SIGKILL");
            kill(child_pid, SIGKILL);
            running = false;
        }
    }
}

int main() {
    safe_cout("Запуск лабораторной 2 (чат)");

    // Определяем базовые имена для ДВУХ направлений
    std::string name_h2c = "/chat_h2c"; // Host -> Client
    std::string name_c2h = "/chat_c2h"; // Client -> Host
    std::string name_heart = "/chat_heart";

    // Очистка старых ресурсов (грубая, но надежная)
    auto cleanup_resource = [](const std::string& name) {
        sem_unlink((name + "_read").c_str());
        sem_unlink((name + "_write").c_str());
        mq_unlink(name.c_str());
        unlink((name + "_read").c_str()); // fifo
        unlink((name + "_write").c_str()); // fifo
    };

    cleanup_resource(name_h2c);
    cleanup_resource(name_c2h);
    cleanup_resource(name_heart);

    // Создаем ресурсы для ОБОИХ направлений
    if (!ConnMq::CreateResources(name_h2c) || !ConnMq::CreateResources(name_c2h) ||
        !ConnFifo::CreateResources(name_h2c) || !ConnFifo::CreateResources(name_c2h) ||
        !ConnSock::CreateResources(name_h2c) || !ConnSock::CreateResources(name_c2h)) {
        safe_cout("Ошибка создания каналов чата"); // Не критично, если используем только MQ, но создаем всё
    }
    
    // Ресурсы для Heartbeat (отдельно)
    ConnFifo::CreateResources(name_heart);

    // socketpair ДО fork (для sock типа)
    int sock_fds_h2c[2];
    int sock_fds_c2h[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds_h2c);
    socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds_c2h);

    pid_t child_pid = fork();
    if (child_pid < 0) {
        safe_cout("fork failed");
        return 1;
    }

    bool is_host = (child_pid != 0);
    last_receive = time(nullptr); // Инициализация времени

    // НАСТРОЙКА КАНАЛОВ
    // Host пишет в H2C, читает из C2H
    // Client пишет в C2H, читает из H2C
    
    std::string my_write_name = is_host ? name_h2c : name_c2h;
    std::string my_read_name  = is_host ? name_c2h : name_h2c;
    
    // Сокеты сложнее, т.к. дескрипторы разные
    int fd_write = is_host ? sock_fds_h2c[0] : sock_fds_c2h[1];
    int fd_read  = is_host ? sock_fds_c2h[0] : sock_fds_h2c[1];
    
    // Закрываем ненужные концы сокетов
    close(is_host ? sock_fds_h2c[1] : sock_fds_h2c[0]);
    close(is_host ? sock_fds_c2h[1] : sock_fds_c2h[0]);

    // Выбираем тип транспорта (здесь жестко MQ для примера, как в твоем логе)
    // Можно поменять на "fifo" или "sock"
    std::string type = "mq"; 
    
    // Создаем объекты
    Conn* conn_write = Conn::create(type, my_write_name, is_host, fd_write);
    Conn* conn_read  = Conn::create(type, my_read_name, is_host, fd_read);
    
    // Heartbeat всегда через FIFO (по заданию часто разные типы)
    Conn* heart_conn = Conn::create("fifo", name_heart, is_host);

    if (!conn_write || !conn_read || !heart_conn) {
        safe_cout("Ошибка создания Conn объектов");
        return 1;
    }

    safe_cout("[" + std::string(is_host ? "HOST" : "CLIENT") + "] Запущен PID: " + std::to_string(getpid()));

    // Запуск потоков
    std::thread input_thr(input_thread, conn_write, is_host);
    std::thread receive_thr(receive_thread, conn_read, is_host);

    if (is_host) {
        std::thread heart_receive(heart_receive_thread, heart_conn);
        std::thread monitor_thr(monitor_thread, child_pid);
        
        monitor_thr.join(); // Ждем завершения монитора (он выйдет если running=false)
        heart_receive.join();
    } else {
        std::thread heartbeat_thr(heartbeat_thread, heart_conn);
        heartbeat_thr.join();
    }

    // Завершение
    input_thr.detach(); // Ввод может висеть на getline, детачим
    receive_thr.join();
    
    delete conn_write;
    delete conn_read;
    delete heart_conn;

    if (is_host) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, nullptr, 0);
        safe_cout("[HOST] Завершение работы.");
    }

    return 0;
}