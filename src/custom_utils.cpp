#include "custom_utils.h"

#include <iostream>
#include <thread>
#include <iomanip>
#include <chrono>
#include <string>
#include <map>
#include <ctime>

namespace custom_utils
{
    using std::cout;

    void print(char character)
    {
        cout << character;
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

    void printNum(double decimal, int decimalPoints)
    {
        cout << std::fixed << std::setprecision(decimalPoints) << decimal;
    }

    const std::map<std::string, int> colorMap = {{"black", 30}, {"red", 31},     {"green", 32}, {"yellow", 33},
                                                 {"blue", 34},  {"magenta", 35}, {"cyan", 36},  {"white", 37}};

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
            if (inputString.at(index) != delimiter)
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

    std::string replaceString(const std::string &original, std::string search, std::string replacement)
    {
        std::string output = "";

        // sliding window
        size_t index = 0;
        size_t start = 0;
        while (index < original.size())
        {
            if (original.at(index) == search.at(start))
            { // keep increasing "start" until "start" = search.size() - 1
                if (start == search.size() - 1)
                { // found search in original
                    index++;
                    start = 0;
                    output += replacement;
                    continue;
                }
                index++;
                start++;
                continue;
            }

            if (start > 0)
            { // partial match, add back the partial part to output
                output += original.substr(index - start, start);
                start = 0;
            }

            output += original.at(index);
            index++;
        }

        return output;
    }

    std::string generate_uuid_string(size_t length)
    {
        std::srand(std::time(0));

        std::string uuid = "";

        for (size_t i = 0; i < length; i++)
        {
            int random_hex = std::rand() % 16;

            switch (random_hex)
            {
            case 10: {
                uuid += "A";
                break;
            }
            case 11: {
                uuid += "B";
                break;
            }
            case 12: {
                uuid += "C";
                break;
            }
            case 13: {
                uuid += "D";
                break;
            }
            case 14: {
                uuid += "E";
                break;
            }
            case 15: {
                uuid += "F";
                break;
            }
            default: {
                uuid += std::to_string(random_hex);
                break;
            }
            }
        }

        return uuid;
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

    void sleep(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
} // namespace custom_utils