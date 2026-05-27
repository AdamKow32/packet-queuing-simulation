cmake_minimum_required(VERSION 3.22)
project(PacketSimulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(packet_sim
        src/cbwfq_scheduler.cpp
        src/fifo_scheduler.cpp
        src/lifo_scheduler.cpp
        src/priority_scheduler.cpp
        src/weighted_round_robin_scheduler.cpp
        src/main.cpp
        src/simulation.cpp
        src/packet.cpp
        src/traffic_generator.cpp
        src/cbwfq_scheduler.h
        src/event.h
        src/fifo_scheduler.h
        src/lifo_scheduler.h
        src/packet.h
        src/priority_scheduler.h
        src/scheduler.h
        src/simulation.h
        src/traffic_generator.h
        src/weighted_round_robin_scheduler.h
        src/statistics.h
        src/statistics.cpp
)

target_include_directories(packet_sim PRIVATE src)
