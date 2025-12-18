# C++ File Server
A lightweight cross-platform file server written in C++.

## Current features
Feature | Description
------- | -----------
FTPS server | Explicit and implicit encryption mode. Support file upload and download. (Only working with FileZilla)

## To be implemented / bug fixes:
* SFTP server
* FTPS public ip mode (currently only works in LAN)
* FTPS mobile download and upload has corrupted files (incorrect size, data)

## Requirements
* CMake version 3.10 or higher
* gcc
* g++
* SQLite3
* Boost.Asio
* OpenSSL
* TLS keys (dh.pem, cert.pem, key.pem)

## How to compile
This project can be compiled using CMake, allowing cross-platform compilation across Windows, macOS and Linux.

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