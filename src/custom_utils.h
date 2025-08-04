#pragma once

#include <chrono>
#include <string>
#include <thread>
#include <map>

using std::string;

namespace custom_utils
{
    void print(string);
    void print(string, string);
    void print(int);
    void print(double, int);

    void setPrintColor(string);
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