#include "custom_utils.hpp"

#include <mutex>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

namespace custom_utils
{
    namespace
    {
        time_t t = time(NULL);
        struct tm *time_struct = localtime(&t);
        int offset_hour = time_struct->tm_gmtoff / 3600;

        std::mutex print_mutex;
    } // namespace

    void print(const std::string &message)
    {
        print(message, COLOR::WHITE);
    }

    void print(const std::string &message, COLOR color)
    {
        print_mutex.lock();

        setPrintColor(color);
        std::cout << '[' << getTimeString() << ']' << ' ' << message;
        resetPrintColor();

        print_mutex.unlock();
    }

    void println()
    {
        println(std::string());
    }

    void println(const std::string &message)
    {
        println(message, COLOR::WHITE);
    }

    void println(const std::string &message, COLOR color)
    {
        print(std::string(message + "\n"), color);
    }

    void setPrintColor(COLOR color)
    {
        std::cout << "\033[" << static_cast<int>(color) << "m";
    }

    void resetPrintColor()
    {
        std::cout << "\033[0m";
    }

    std::string getTimeString()
    {
        time_t t = time(NULL);
        std::string h = std::to_string(((t / 3600) + offset_hour) % 24);
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
