#include "src/customUtils.h"
#include "src/SQLite_db.h"

int main()
{
    stopwatch performanceWatch;
    performanceWatch.start();

    SQLiteDb database = SQLiteDb();

    print("Done in ");
    print(performanceWatch.lapMs());
    print("ms\n");
    return 0;
}
