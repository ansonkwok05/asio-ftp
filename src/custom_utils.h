#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>

namespace custom_utils
{
    /**
     * print message to console
     * @param message string to print to console
     */
    void print(std::string message);

    /**
     * print message to console with color
     * @param message string to print to console
     * @param color int representing the color (30 - 97)
     */
    void print(std::string message, int color);

    /**
     * print empty line to console
     */
    void println();

    /**
     * print message to console with line break
     * @param message string to print to console
     */
    void println(std::string message);

    /**
     * print message to console with line break and color
     * @param message string to print to console
     */
    void println(std::string message, int color);

    enum COLORS
    {
        BLACK = 30,
        RED = 31,
        GREEN = 32,
        YELLOW = 33,
        BLUE = 34,
        MAGENTA = 35,
        CYAN = 36,
        WHITE = 37,
        BRIGHTBLACK = 90,
        BRIGHTRED = 91,
        BRIGHTGREEN = 92,
        BRIGHTYELLOW = 93,
        BRIGHTBLUE = 94,
        BRIGHTMAGENTA = 95,
        BRIGHTCYAN = 96,
        BRIGHTWHITE = 97,
    };

    /**
     * set the color of cout by using ANSI color code
     * @param color int representing the color (30 - 97)
     */
    void setPrintColor(int color);

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