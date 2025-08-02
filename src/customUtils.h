#pragma once

#include <chrono>
#include <string>
#include <thread>
#include <map>

namespace customUtils
{
    void print(std::string);
    void print(int);
    void print(double, int);

    void setPrintColor(std::string);
    void resetPrintColor();

    class stopwatch
    {
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
}