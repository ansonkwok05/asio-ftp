#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

namespace custom_utils
{
    void print(std::string message);
    void print(std::string message, std::string color);

    void println();
    void println(std::string message);
    void println(std::string message, std::string color);

    const std::map<std::string, int> COLORS = {
        {"black", 30},       {"red", 31},           {"green", 32},       {"yellow", 33},
        {"blue", 34},        {"magenta", 35},       {"cyan", 36},        {"white", 37},
        {"brightblack", 90}, {"brightred", 91},     {"brightgreen", 92}, {"brightyellow", 93},
        {"brightblue", 94},  {"brightmagenta", 95}, {"brightcyan", 96},  {"brightwhite", 97}};

    /**
     * set the color of cout by using ANSI color code
     * @param color A string representing the color (black, red, green, yellow, blue, magenta, cyan, white)
     */
    void setPrintColor(std::string color);

    void resetPrintColor();

    std::string getTimeString();

    std::vector<std::string> splitString(const std::string &inputString, char delimiter);
    std::string vectorStrJoin(std::vector<std::string> inputVector, std::string seperator);

    std::string strToUpper(const std::string input);
    bool strStartsWith(const std::string input, std::string prefix);

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