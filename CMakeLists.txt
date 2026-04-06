cmake_minimum_required(VERSION 3.22)
project(PacketSimulator CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(packet_sim
        src/main.cpp
)

target_include_directories(packet_sim PRIVATE src)