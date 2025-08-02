#include "src/customUtils.h"
#include "src/sqlite_wrapper.h"

using customUtils::print;
using customUtils::resetPrintColor;
using customUtils::setPrintColor;

int main()
{
    customUtils::stopwatch performanceTimer;
    performanceTimer.start();

    setPrintColor("Green");
    print("Initializing database\n");
    resetPrintColor();

    sqlite_wrapper::SQLiteDb database = sqlite_wrapper::SQLiteDb();

    setPrintColor("Green");
    print("Database setup done in ");
    print(performanceTimer.lapUs() / 1000, 3);
    print("ms\n");
    resetPrintColor();
    return 0;
}
