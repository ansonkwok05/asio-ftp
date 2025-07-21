#pragma once

#include <string>
#include <chrono>
#include <thread>

void print(std::string);
void print(double);

class stopwatch {
    public:
        stopwatch();
        void start();
        double lapMs();
        double lapUs();
        double lapNs();
        
    private:
        std::chrono::time_point<std::chrono::steady_clock> timeStamp;
};

void sleep(int);