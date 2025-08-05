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

    // // test insert data
    // performanceTimer.start();

    // print("Restarted timer ", "blue");
    // print(performanceTimer.lapUs() / 1000, 3);
    // print("ms\n");

    // std::string testStr = "XD";
    // for (int i = 0; i < 10; i++)
    // {
    //     database.insert_data("users", {"userid", "username", "password"}, {testStr, "huhlmao", "pw"});
    //     testStr += "D";
    //     print(performanceTimer.lapUs() / 1000, 3);
    //     print("ms\n");
    // }

    // print("10 insert operation all done in ", "blue");
    // print(performanceTimer.lapUs() / 1000, 3);
    // print("ms\n");

    return 0;
}
