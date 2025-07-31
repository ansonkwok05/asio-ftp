#!/bin/bash

# compiling SQLite3 library
echo "building ./third_party/sqlite3.c" && gcc -fdiagnostics-color=always -g -c "./third_party/sqlite3.c" -o "./build/sqlite3.o"

# compiling .cpp files
find . -name "*.cpp" -print0 | xargs -0 -P $(nproc) -I {} bash -c 'filename=$(basename {}) && echo "building {}" && g++ -fdiagnostics-color=always -g -c {} -o "./build/${filename%.*}.o"'

# linking .o files
echo "linking object files"
g++ -fdiagnostics-color=always -g ./build/*.o -o main

echo "compilation done"