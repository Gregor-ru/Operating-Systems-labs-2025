cmake_minimum_required(VERSION 3.10)
project(lab2_var13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pthread")

set(HOST_SRC host.cpp)

# 1. Message Queues
add_executable(host_mq ${HOST_SRC} conn_mq.cpp)
target_link_libraries(host_mq rt pthread)

# 2. Named Pipes
add_executable(host_fifo ${HOST_SRC} conn_fifo.cpp)
target_link_libraries(host_fifo rt pthread)

# 3. Sockets
add_executable(host_sock ${HOST_SRC} conn_sock.cpp)
target_link_libraries(host_sock rt pthread)