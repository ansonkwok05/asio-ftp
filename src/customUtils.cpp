#include "customUtils.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>

void print(std::string message) {
    std::cout << message;
}
void print(double duration) {
    std::cout << duration;
}

void exception(std::string message) {
    throw std::runtime_error(message);
}

stopwatch::stopwatch() {}

void stopwatch::start() {
    timeStamp = std::chrono::steady_clock::now();
}

double stopwatch::lapMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeStamp).count();
}
double stopwatch::lapUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - timeStamp).count();
}
double stopwatch::lapNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - timeStamp).count();
}

void sleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}