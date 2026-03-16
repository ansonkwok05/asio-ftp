# asio-ftp
A small FTP file server written with Boost.Asio

## Description
- Explicit or implicit encryption through the same port.
- File download/upload using FileZilla.

## To be implemented / bug fixes:
* Dont know how to deal with public ips (only works in LAN now)
* Some clients has corrupted files from downloads and uploads (incorrect file size, corrupted data)
* Multithreading has random segfaults

## How to compile
This project can be compiled using CMake.

### Requirements
* CMake version 3.10 or higher
* gcc
* g++
* SQLite3
* Boost.Asio
* OpenSSL
* TLS certificate (dh.pem, cert.pem, key.pem)

```
mkdir build
cd ./build
cmake ..
make
```