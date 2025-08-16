cmake_minimum_required(VERSION 3.10)

project(QuickShare)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

file(GLOB src
    "src/*.cpp"
)
file(GLOB third_party
    "third_party/*.c"
)

add_executable(QuickShare main.cpp ${src} ${third_party})

include_directories("src" "third_party")