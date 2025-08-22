#include "custom_utils.h"

#include <iostream>
#include <thread>
#include <iomanip>
#include <chrono>
#include <string>
#include <map>

namespace custom_utils
{
    using std::cout;

    void print(char message)
    {
        cout << message;
    }

    void print(std::string message)
    {
        cout << message;
    }

    void print(std::string message, std::string color)
    {
        setPrintColor(color);
        cout << message;
        resetPrintColor();
    }

    void printNum(int number)
    {
        cout << std::fixed << std::setprecision(0) << number;
    }

    /**
     * @param decimal A decimal number
     * @param decimalPoints Decimal points to display
     */
    void printNum(double decimal, int decimalPoints)
    {
        cout << std::fixed << std::setprecision(decimalPoints) << decimal;
    }

    const std::map<std::string, int> colorMap = {{"black", 30}, {"red", 31},     {"green", 32}, {"yellow", 33},
                                                 {"blue", 34},  {"magenta", 35}, {"cyan", 36},  {"white", 37}};

    /**
     * set the color of cout by using ANSI color code
     * @param color A string representing the color (black, red, green, yellow, blue, magenta, cyan, white)
     */
    void setPrintColor(std::string color)
    {
        cout << "\033[" << colorMap.at(color) << "m";
    }

    void resetPrintColor()
    {
        cout << "\033[0m";
    }

    std::vector<std::string> splitString(const std::string &inputString, char delimiter)
    {
        if (inputString.size() == 0)
            return {};

        std::vector<std::string> splittedStr;

        size_t start = 0;
        size_t index = 0;
        while (index < inputString.size())
        {
            if (inputString.at(index) != ' ')
            {
                index++;
                continue;
            }
            splittedStr.push_back(inputString.substr(start, index - start));
            start = index + 1;

            index++;
        }
        splittedStr.push_back(inputString.substr(start, index + 1));

        return splittedStr;
    }

    custom_utils::stopwatch::stopwatch()
    {
    }

    void stopwatch::start()
    {
        timeStamp = std::chrono::steady_clock::now();
    }

    double stopwatch::lapMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeStamp)
            .count();
    }

    double stopwatch::lapUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - timeStamp)
            .count();
    }

    double stopwatch::lapNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - timeStamp)
            .count();
    }

    /**
     * sleep for milliseconds
     * @param ms An int representing miliseconds
     */
    void sleep(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
} // namespace custom_utils