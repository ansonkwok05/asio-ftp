#include "src/custom_utils.h"
#include "src/ftps/ftps_server.h"
#include "src/sqlite/sqlite_wrapper.h"

#include <thread>
#include <vector>
#include <unordered_map>

using custom_utils::print;
using custom_utils::printNum;

int main()
{
    { // database initialization
        custom_utils::stopwatch performanceWatcher;
        performanceWatcher.start();

        print("Initializing database\n", "green");

        sqlite_wrapper::SQLiteDb *database = new sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING);
        database->init_db();

        print("Database initialization done in ", "green");
        printNum(performanceWatcher.lapUs() / 1000, 3);
        print("ms\n");

        { // test database insert data
            performanceWatcher.start();
            print("\nStarting insert test\n", "blue");
            std::string useridStr = "XD";
            std::string usernameStr = "lmaoxd";
            for (int i = 0; i < 10; i++)
            {
                useridStr += "D";
                usernameStr += "1";
                database->insert_data("users", {"user_id", "username", "password"}, {useridStr, usernameStr, "pw"});
                printNum(performanceWatcher.lapUs() / 1000, 3);
                print("ms ");
            }
            print("\n10 insert operation all done in ", "blue");
            printNum(performanceWatcher.lapUs() / 1000, 3);
            print("ms\n");
        }

        { // test database read data
            performanceWatcher.start();
            print("\nStarting read test\n", "blue");
            for (int i = 0; i < 10; i++)
            {
                std::vector<std::string> return_data;
                database->read_data("users", {}, return_data);
                printNum(performanceWatcher.lapUs() / 1000, 3);
                print("ms ");
            }
            print("\n10 read operation all done in ", "blue");
            printNum(performanceWatcher.lapUs() / 1000, 3);
            print("ms\n");
        }

        { // test database delete data
            performanceWatcher.start();
            print("\nStarting delete test\n", "blue");
            std::string useridStr = "XD";
            for (int i = 0; i < 10; i++)
            {
                useridStr += "D";
                database->delete_data("users", "user_id", useridStr);
                printNum(performanceWatcher.lapUs() / 1000, 3);
                print("ms ");
            }
            print("\n10 delete operation all done in ", "blue");
            printNum(performanceWatcher.lapUs() / 1000, 3);
            print("ms\n");
        }

        { // create test data
            print("\nCreating test data\n", "blue");

            { // create test data for "users" table
                std::vector<std::string> return_data;
                database->read_data("users", {}, return_data);

                std::unordered_map<std::string, std::string> parsedData;
                size_t i = 0;
                while (i < return_data.size())
                {
                    parsedData[return_data.at(i)] = return_data.at(i + 1);
                    i += 3;
                }

                if (parsedData.find("numba1") == parsedData.end())
                {
                    database->insert_data("users", {"user_id", "username", "password"}, {"numba1", "useless1", "test"});
                }

                if (parsedData.find("numba2") == parsedData.end())
                {
                    database->insert_data("users", {"user_id", "username", "password"}, {"numba2", "useless2", "test"});
                }
            }

            { // create test data for "files" table
                std::vector<std::string> return_data;
                database->read_data("files", {}, return_data);

                std::vector<std::string> parsedData;
                size_t i = 0;
                while (i < return_data.size())
                {
                    parsedData.push_back(return_data.at(i));
                    i += 2;
                }

                if (std::find(parsedData.begin(), parsedData.end(), "test_file_1") == parsedData.end())
                {
                    database->insert_data("files", {"user_id", "file_id"}, {"numba1", "test_file_1"});
                }

                if (std::find(parsedData.begin(), parsedData.end(), "test_file_2") == parsedData.end())
                {
                    database->insert_data("files", {"user_id", "file_id"}, {"numba1", "test_file_2"});
                }
            }

            { // create test data for "files_metadata" table
                std::vector<std::string> return_data;
                database->read_data("files_metadata", {}, return_data);

                std::vector<std::string> parsedData;
                size_t i = 4;
                while (i < return_data.size())
                {
                    parsedData.push_back(return_data.at(i));
                    i += 5;
                }

                if (std::find(parsedData.begin(), parsedData.end(), "test_file_1") == parsedData.end())
                {
                    database->insert_data("files_metadata", {"file_name", "file_path", "file_size", "file_id"},
                                          {"yo_iden", "/", "21", "test_file_1"});
                }

                if (std::find(parsedData.begin(), parsedData.end(), "test_file_2") == parsedData.end())
                {
                    database->insert_data("files_metadata", {"file_name", "file_path", "file_size", "file_id"},
                                          {"soda", "/", "9", "test_file_2"});
                }
            }

            print("Test data created\n", "blue");
        }

        delete database;
        database = nullptr;
    }

    print("\n");

    { // launch FTPS server in worker thread
        std::thread ftps_server_thread([]() { ftps_server::server ftps_server = ftps_server::server(); });

        print("FTPS server worker thread started\n", "green");

        ftps_server_thread.join(); // prevent main thread to die, while worker thread can do its thing
    }

    return 0;
}
