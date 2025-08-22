#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

namespace custom_utils
{
    void print(char);
    void print(std::string);
    void print(std::string, std::string);
    void printNum(int);
    void printNum(double, int);

    void setPrintColor(std::string);
    void resetPrintColor();

    std::vector<std::string> splitString(const std::string &, char);

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
} // namespace custom_utils