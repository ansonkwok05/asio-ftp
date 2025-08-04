#include "src/custom_utils.h"
#include "src/sqlite_wrapper.h"

using custom_utils::print;

int main()
{
    custom_utils::stopwatch performanceTimer;
    performanceTimer.start();

    print("Initializing database\n", "green");

    sqlite_wrapper::SQLiteDb database = sqlite_wrapper::SQLiteDb();

    print("Database setup done in ", "green");
    print(performanceTimer.lapUs() / 1000, 3);
    print("ms\n");
    return 0;
}
