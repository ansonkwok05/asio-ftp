#include "src/customUtils.h"
#include "src/SQLite_db.h"

int main() {
    stopwatch performanceWatch;
    performanceWatch.start();


    SQLiteDb database = SQLiteDb("storage");



    print("Done in ");
    print(performanceWatch.lapUs());
    print("us\n");
    return 0;
}
