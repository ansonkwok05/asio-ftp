#include "src/custom_utils.h"
#include "src/sqlite_wrapper.h"
#include "src/ftp_server.h"

using custom_utils::print;

int main()
{
    custom_utils::stopwatch performanceWatcher;
    performanceWatcher.start();

    print("Initializing database\n", "green");

    sqlite_wrapper::SQLiteDb database = sqlite_wrapper::SQLiteDb();

    print("Database setup done in ", "green");
    print(performanceWatcher.lapUs() / 1000, 3);
    print("ms\n");

    // {
    //     // test database insert data
    //     performanceWatcher.start();
    //     print("\nRestarted timer, starting insert test ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    //     std::string useridStr = "XD";
    //     for (int i = 0; i < 10; i++)
    //     {
    //         database.insert_data("users", {"userid", "username", "password"}, {useridStr, "huhlmao", "pw"});
    //         useridStr += "D";
    //         print(performanceWatcher.lapUs() / 1000, 3);
    //         print("ms ");
    //     }
    //     print("\n");
    //     print("10 insert operation all done in ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    // }

    // {
    //     // test database read data
    //     performanceWatcher.start();
    //     print("\nRestarted timer, starting read test ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    //     for (int i = 0; i < 10; i++)
    //     {
    //         database.read_data("users", {"userid", "username"});
    //         print(performanceWatcher.lapUs() / 1000, 3);
    //         print("ms ");
    //     }
    //     print("\n");
    //     print("10 read operation all done in ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    // }

    // {
    //     // test database delete data
    //     performanceWatcher.start();
    //     print("\nRestarted timer, starting delete test ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    //     std::string useridStr = "XD";
    //     for (int i = 0; i < 10; i++)
    //     {
    //         database.delete_data("users", "userid", useridStr);
    //         useridStr += "D";
    //         print(performanceWatcher.lapUs() / 1000, 3);
    //         print("ms ");
    //     }
    //     print("\n");
    //     print("10 delete operation all done in ", "blue");
    //     print(performanceWatcher.lapUs() / 1000, 3);
    //     print("ms\n");
    // }

    /* test data
    INSERT INTO users (userid, username, password) VALUES ("uid", "un", "pw");
    INSERT INTO files (fileid, userid) VALUES ("fid", "uid");
    INSERT INTO files_metadata (filename, file_description, file_size, fileid) VALUES ("fn", "descript", "123", "fid");
    */

    //
    print("\n");
    //

    performanceWatcher.start();

    print("Initializing FTP server\n", "green");

    ftp_server::server ftp_server = ftp_server::server();

    print("FTP server setup done in ", "green");
    print(performanceWatcher.lapUs() / 1000, 3);
    print("ms\n");

    return 0;
}
