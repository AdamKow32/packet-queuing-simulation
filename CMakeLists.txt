cmake_minimum_required(VERSION 3.22)
project(PacketSimulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(packet_sim
        src/fifo_scheduler.cpp
        src/main.cpp
        src/simulation.cpp
        src/packet.cpp
        src/traffic_generator.cpp
        src/event.h
        src/fifo_scheduler.h
        src/packet.h
        src/scheduler.h
        src/simulation.h
        src/traffic_generator.h
        src/statistics.h
        src/statistics.cpp
)

target_include_directories(packet_sim PRIVATE src)
