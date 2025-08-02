cmake_minimum_required(VERSION 3.10)

project(QuickShare)

file(GLOB src
    "src/*.h"
    "src/*.cpp"
)
file(GLOB third_party
    "third_party/*.h"
    "third_party/*.c"
)

add_executable(QuickShare main.cpp ${src} ${third_party})