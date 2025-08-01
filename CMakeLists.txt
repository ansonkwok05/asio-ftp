cmake_minimum_required(VERSION 3.10)

project(QuickShare)

add_executable(QuickShare main.cpp third_party/sqlite3.c src/customUtils.cpp src/SQLite_db.cpp)