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

        println("Initializing database", custom_utils::COLORS::GREEN);

        sqlite_wrapper::SQLiteDb *database = new sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING);
        database->init_db();

        println("Database initialization done in " + std::to_string(performanceWatcher.lapUs() / 1000) + "ms",
                custom_utils::COLORS::GREEN);

        { // create user data if not exists
            {
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

            println("Test data created", custom_utils::COLORS::BLUE);
        }

        delete database;
        database = nullptr;
    }

    // launch FTPS server in worker thread
    std::thread ftps_server_thread([]() { ftps_server::server ftps_server = ftps_server::server(); });

    println("FTPS server worker thread started", custom_utils::COLORS::GREEN);
    ftps_server_thread.join(); // prevent main thread to die, while worker thread can do its thing

    return 0;
}
