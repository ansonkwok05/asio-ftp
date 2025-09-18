# QuickSave
QuickSave is a cross-platform C++ application built to provide a fast and straightforward way to share files directly between devices.

### Current features
* SQLite database operations - insert, read, delete
* FTPS server - explicit and implicit mode

### To be implemented:
* SFTP server

# Requirements
* CMake version 3.10 or higher
* gcc
* g++
* SQLite3
* Boost.Asio
* OpenSSL

# Compilation
This project is built using CMake, allowing cross-platform compilation across Windows, macOS and Linux.

### Windows
Can be built using Visual Studio, or use CMake to generate your own build files.

### Linux
```
mkdir build
cd ./build
cmake ..
make
```

### macOS
Not tested, but should work by using CMake to build.