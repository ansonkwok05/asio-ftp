#include "custom_utils.h"

#include <iostream>
#include <thread>
#include <iomanip>
#include <chrono>
#include <string>
#include <map>

namespace custom_utils
{
    time_t t = time(NULL);
    struct tm *time_struct = localtime(&t);
    int offset_hour = time_struct->tm_gmtoff / 3600;

    using std::cout;

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

    void println()
    {
        cout << '[' << getTimeString() << ']' << ' ';
        cout << "\n";
    }

    void println(std::string message)
    {
        cout << '[' << getTimeString() << ']' << ' ';
        cout << message << "\n";
    }

    void println(std::string message, std::string color)
    {
        setPrintColor(color);
        cout << '[' << getTimeString() << ']' << ' ';
        cout << message << "\n";
        resetPrintColor();
    }

    void setPrintColor(std::string color)
    {
        cout << "\033[" << COLORS.at(color) << "m";
    }

    void resetPrintColor()
    {
        cout << "\033[0m";
    }

    std::string getTimeString()
    {
        time_t t = time(NULL);
        std::string h = std::to_string((t / 3600) % 24 + offset_hour);
        std::string m = std::to_string((t / 60) % 60);
        std::string s = std::to_string(t % 60);

        if (h.size() < 2)
            h = '0' + h;

        if (m.size() < 2)
            m = '0' + m;

        if (s.size() < 2)
            s = '0' + s;

        return h + ':' + m + ':' + s;
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

    std::string vectorStrJoin(std::vector<std::string> inputVector, std::string seperator)
    {
        std::string output;

        for (size_t i = 0; i < inputVector.size() - 1; i++)
        {
            output += inputVector.at(i) + seperator;
        }
        output += inputVector.at(inputVector.size() - 1);

        return output;
    }

    std::string strToUpper(const std::string input)
    {
        std::string output = "";

        size_t i = 0;
        while (i < input.size())
        {
            if (input.at(i) >= 'a' && input.at(i) <= 'z')
            {
                output += input.at(i) - 32;
            }
            else
            {
                output += input.at(i);
            }

            i++;
        }

        return output;
    }

    bool strStartsWith(const std::string input, std::string prefix)
    {
        if (input.size() < prefix.size())
        {
            return false;
        }

        size_t i = 0;
        while (i < prefix.size())
        {
            if (input.at(i) != prefix.at(i))
            {
                return false;
            }
            i++;
        }

        return true;
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