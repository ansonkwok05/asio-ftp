#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

namespace custom_utils
{
    void print(char character);
    void print(std::string message);
    void print(std::string message, std::string color);

    void printNum(int number);

    /**
     * @param decimal A decimal number
     * @param decimalPoints Decimal points to display
     */
    void printNum(double decimal, int decimalPoints);

    /**
     * set the color of cout by using ANSI color code
     * @param color A string representing the color (black, red, green, yellow, blue, magenta, cyan, white)
     */
    void setPrintColor(std::string color);

    void resetPrintColor();

    std::vector<std::string> splitString(const std::string &inputString, char delimiter);
    std::string vectorStrJoin(std::vector<std::string> inputVector, std::string seperator);

    std::string replaceString(const std::string &original, std::string search, std::string replacement);
    std::string generate_uuid_string(size_t length);

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

    /**
     * sleep for milliseconds
     * @param ms An int representing miliseconds
     */
    void sleep(int ms);
} // namespace custom_utils