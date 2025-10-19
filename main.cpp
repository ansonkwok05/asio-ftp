#include "src/custom_utils.h"
#include "src/ftps/ftps_server.h"
#include "src/sqlite/sqlite_wrapper.h"

#include <thread>
#include <vector>
#include <unordered_map>

using custom_utils::println;

int main()
{
    std::srand(std::time(NULL)); // initialize rand

    { // database initialization
        custom_utils::stopwatch performanceWatcher;
        performanceWatcher.start();

        println("Initializing database", "green");

        sqlite_wrapper::SQLiteDb *database = new sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING);
        database->init_db();

        println("Database initialization done in " + std::to_string(performanceWatcher.lapUs() / 1000) + "ms", "green");

        // { // test database insert data
        //     performanceWatcher.start();
        //     println("Starting insert test", "blue");

        //     std::string useridStr = "XD";
        //     std::string usernameStr = "lmaoxd";

        //     std::string log = "";
        //     for (int i = 0; i < 10; i++)
        //     {
        //         useridStr += "D";
        //         usernameStr += "1";
        //         database->insert_data("users", {"user_id", "username", "password"}, {useridStr, usernameStr,
        //         "pw"}); log += std::to_string(performanceWatcher.lapUs() / 1000) + "ms ";
        //     }
        //     println(log);

        //     println("10 insert operation all done in " + std::to_string(performanceWatcher.lapUs() / 1000) +
        //     "ms",
        //             "blue");
        // }

        // println();

        // { // test database read data
        //     performanceWatcher.start();
        //     println("Starting read test", "blue");

        //     std::string log = "";
        //     for (int i = 0; i < 10; i++)
        //     {
        //         std::vector<std::string> return_data;
        //         database->read_data("users", {}, return_data);
        //         log += std::to_string(performanceWatcher.lapUs() / 1000) + "ms ";
        //     }
        //     println(log);
        //     println("10 read operation all done in " + std::to_string(performanceWatcher.lapUs() / 1000) + "ms",
        //             "blue");
        // }

        // println();

        // { // test database delete data
        //     performanceWatcher.start();
        //     println("Starting delete test", "blue");

        //     std::string useridStr = "XD";

        //     std::string log = "";
        //     for (int i = 0; i < 10; i++)
        //     {
        //         useridStr += "D";
        //         database->delete_data("users", "user_id", useridStr);
        //         log += std::to_string(performanceWatcher.lapUs() / 1000) + "ms ";
        //     }
        //     println(log);

        //     println("10 delete operation all done in " + std::to_string(performanceWatcher.lapUs() / 1000) +
        //     "ms",
        //             "blue");
        // }

        {     // create test data
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

            // { // create test data for "files" table
            //     std::vector<std::string> return_data;
            //     database->read_data("files", {}, return_data);

            //     std::vector<std::string> parsedData;
            //     size_t i = 0;
            //     while (i < return_data.size())
            //     {
            //         parsedData.push_back(return_data.at(i));
            //         i += 2;
            //     }

            //     std::string file_idstr = "test_file";

            //     for (int i = 0; i < 5; i++)
            //     {
            //         file_idstr += "1";
            //         if (std::find(parsedData.begin(), parsedData.end(), file_idstr) == parsedData.end())
            //         {
            //             database->insert_data("files", {"user_id", "file_id"}, {"numba1", file_idstr});
            //         }
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "folder_id1") == parsedData.end())
            //     {
            //         database->insert_data("files", {"user_id", "file_id"}, {"numba1", "folder_id1"});
            //     }

            //     for (int i = 0; i < 5; i++)
            //     {
            //         file_idstr += "1";
            //         if (std::find(parsedData.begin(), parsedData.end(), file_idstr) == parsedData.end())
            //         {
            //             database->insert_data("files", {"user_id", "file_id"}, {"numba1", file_idstr});
            //         }
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "folder_id2") == parsedData.end())
            //     {
            //         database->insert_data("files", {"user_id", "file_id"}, {"numba1", "folder_id2"});
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "special_file1") == parsedData.end())
            //     {
            //         database->insert_data("files", {"user_id", "file_id"}, {"numba1", "special_file1"});
            //     }
            // }

            // { // create test data for "files_metadata" table
            //     std::vector<std::string> return_data;
            //     database->read_data("files_metadata", {}, return_data);

            //     std::vector<std::string> parsedData;
            //     size_t i = 5;
            //     while (i < return_data.size())
            //     {
            //         parsedData.push_back(return_data.at(i));
            //         i += 6;
            //     }

            //     std::string file_idstr = "test_file";

            //     for (int i = 0; i < 5; i++)
            //     {
            //         file_idstr += "1";
            //         if (std::find(parsedData.begin(), parsedData.end(), file_idstr) == parsedData.end())
            //         {
            //             database->insert_data("files_metadata",
            //                                   {"file_name", "file_path", "file_size", "is_directory", "file_id"},
            //                                   {"yo_iden" + file_idstr, "/", "21", "0", file_idstr});
            //         }
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "folder_id1") == parsedData.end())
            //     {
            //         database->insert_data("files_metadata",
            //                               {"file_name", "file_path", "file_size", "is_directory", "file_id"},
            //                               {"example_folder", "/", "9", "1", "folder_id1"});
            //     }

            //     for (int i = 0; i < 5; i++)
            //     {
            //         file_idstr += "1";
            //         if (std::find(parsedData.begin(), parsedData.end(), file_idstr) == parsedData.end())
            //         {
            //             database->insert_data("files_metadata",
            //                                   {"file_name", "file_path", "file_size", "is_directory", "file_id"},
            //                                   {"yo_iden" + file_idstr, "/example_folder", "21", "0",
            //                                   file_idstr});
            //         }
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "folder_id2") == parsedData.end())
            //     {
            //         database->insert_data("files_metadata",
            //                               {"file_name", "file_path", "file_size", "is_directory", "file_id"},
            //                               {"example_folder in a folder", "/example_folder", "0", "1",
            //                               "folder_id2"});
            //     }

            //     if (std::find(parsedData.begin(), parsedData.end(), "special_file1") == parsedData.end())
            //     {
            //         database->insert_data(
            //             "files_metadata", {"file_name", "file_path", "file_size", "is_directory", "file_id"},
            //             {"special_file", "/example_folder/example_folder in a folder", "6969", "0",
            //             "special_file1"});
            //     }
            // }

            println("Test data created", "blue");
        }

        delete database;
        database = nullptr;
    }

    // launch FTPS server in worker thread
    std::thread ftps_server_thread([]() { ftps_server::server ftps_server = ftps_server::server(); });

    println("FTPS server worker thread started", "green");
    ftps_server_thread.join(); // prevent main thread to die, while worker thread can do its thing

    return 0;
}
