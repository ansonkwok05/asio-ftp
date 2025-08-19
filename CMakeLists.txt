cmake_minimum_required(VERSION 3.10)

project(QuickSave)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

file(GLOB src
    "src/*.cpp"
)

add_executable(QuickSave main.cpp ${src})

# SQLite3 dependency
find_package(SQLite3 REQUIRED)
if (SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIRS})
    target_link_libraries(QuickSave sqlite3)
else()
    message(FATAL_ERROR "SQLite3 not found. Please install it using a package manager")
endif()

# libssh dependency
find_package(libssh REQUIRED)
if (libssh_FOUND)
    include_directories(${libssh_INCLUDE_DIRS})
    target_link_libraries(QuickSave ssh)
else()
    message(FATAL_ERROR "libssh not found. Please install it using a package manager")
endif()