# asio-ftp
FTP file server written with Boost.Asio

## Features
- Support file download/upload using FileZilla
- Non-blocking and multi-threaded I/O handling
- Optional explicit and implicit encryption

## To be implemented / issues:
* handle public ip
* Some FTP clients has corrupted files from downloads and uploads (incorrect file size, corrupted data)
* multi-threading is very unstable

## Requirements
* CMake version >= 3.30
* SQLite3
* Boost (Asio, JSON)
* OpenSSL

## To build and run
This project can be compiled using CMake
```
git clone https://github.com/ansonkwok05/asio-ftp
cd asio-ftp
mkdir build && cd build
cmake ..
make
./asio-ftp
```

The program uses no encryption by default. To use encryption, TLS certificate must be provided.

### Configure
The program creates a default json file at ./config/config.json when no config file is found. Modify this file to configure program behavior.