# QuickShare
A software that shares files across devices with portability and user-friendly in mind.

# Requirements
* **CMake** version 3.10 or higher
* **gcc**

# Compilation
This project is built using CMake, allowing cross-platform compilation across Windows, macOS and Linux.

### Windows
Build using Visual Studio, or use CMake to generate your own build files.<br>
Using CMake in Windows is pretty much the same as in Linux, the only problem is setting the right PATH environment.

### Linux
```
mkdir build
cd ./build
cmake ..
make
```

### macOS
Not tested, but should work using CMake.