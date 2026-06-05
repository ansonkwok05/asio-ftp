# asio-ftp
FTP file server written in C++

## Features
- Support file download/upload using FileZilla
- Non-blocking I/O handling
- Optional explicit and implicit encryption

## To be implemented / issues:
* handle public ip
* Some FTP clients has corrupted files from downloads and uploads (incorrect file size, corrupted data)

## Requirements
* CMake (version >= 3.30)
* sqlite3
* boost-asio
* boost-json
* openssl

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

### Configuration
The program creates a default json file at ./config/config.json when no config file is found. Modify this file to configure program behavior.