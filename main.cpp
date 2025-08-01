#include "src/SQLite_db.h"
#include "src/customUtils.h"

int main()
{
    stopwatch performanceTimer;
    performanceTimer.start();

    SQLiteDb database = SQLiteDb();

    print("Done in ");
    print(performanceTimer.lapMs());
    print("ms\n");
    return 0;
}
