#include "customUtils.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

namespace customUtils
{
	void print(std::string message)
	{
		std::cout << message;
	}

	void print(int number)
	{
		std::cout << std::fixed << std::setprecision(0) << number;
	}

	/**
	 * @param decimal A decimal number
	 * @param decimalPoints Decimal points to display
	 */
	void print(double decimal, int decimalPoints)
	{
		std::cout << std::fixed << std::setprecision(decimalPoints) << decimal;
	}

	const std::map<std::string, int> colorMap = {
		{"black", 30}, {"red", 31}, {"green", 32}, {"yellow", 33}, {"blue", 34}, {"magenta", 35}, {"cyan", 36}, {"white", 37}};

	/**
	 * set the color of cout by setting ANSI color code
	 * @param color A string representing the color (Black, Red, Green, Yellow,
	 * Blue, Magenta, Cyan, White)
	 */
	void setPrintColor(std::string color)
	{
		std::cout << "\033[" << colorMap.at(color) << "m";
	}

	void resetPrintColor()
	{
		std::cout << "\033[0m";
	}

	customUtils::stopwatch::stopwatch() {}

	void stopwatch::start()
	{
		timeStamp = std::chrono::steady_clock::now();
	}

	double stopwatch::lapMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeStamp).count();
	}

	double stopwatch::lapUs()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - timeStamp).count();
	}

	double stopwatch::lapNs()
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - timeStamp).count();
	}

	void sleep(int ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
} // namespace customUtils