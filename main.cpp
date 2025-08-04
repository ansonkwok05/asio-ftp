#include "src/customUtils.h"
#include "src/sqlite_wrapper.h"

using customUtils::print;
using customUtils::resetPrintColor;
using customUtils::setPrintColor;

int main()
{
    customUtils::stopwatch performanceTimer;
    performanceTimer.start();

    print("Initializing database\n", "green");

    sqlite_wrapper::SQLiteDb database = sqlite_wrapper::SQLiteDb();

    setPrintColor("green");
    print("Database setup done in ");
    print(performanceTimer.lapUs() / 1000, 3);
    print("ms\n");
    resetPrintColor();
    return 0;
}
