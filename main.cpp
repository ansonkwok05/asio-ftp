#include "src/custom_utils.h"
#include "src/database/sqlite_wrapper.h"
#include "src/database/user_db.h"
#include "src/database/virtual_fs_db.h"
#include "src/ftps/ftps_server.h"
// #include "src/sftp/sftp_server.h"

#include <thread>
// #include <vector>

using custom_utils::println;

int main()
{
    // initialize rand
    std::srand(std::time(NULL));

    custom_utils::stopwatch performanceWatcher;

    {
        performanceWatcher.start();
        println("Initializing SQLite database", custom_utils::COLOR::GREEN);

        sqlite_wrapper::SQLiteDb *database = new sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING);
        database->init_db();
        delete database;
        database = nullptr;

        println("SQLite database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) +
                    "ms",
                custom_utils::COLOR::GREEN);
    }

    {
        performanceWatcher.start();
        println("Initializing user database", custom_utils::COLOR::GREEN);

        user_db::user *user = new user_db::user();
        user->initialize();

        if (user->get_id_by_name("useless1").size() == 0)
        {
            println("Attemping to create user \"useless1\"", custom_utils::COLOR::YELLOW);
            user->create_user("useless1", "test");
            println("Created user \"useless1\"", custom_utils::COLOR::GREEN);
        }

        if (user->get_id_by_name("useless2").size() == 0)
        {
            println("Attemping to create user \"useless2\"", custom_utils::COLOR::YELLOW);
            user->create_user("useless2", "test");
            println("Created user \"useless2\"", custom_utils::COLOR::GREEN);
        }

        delete user;
        user = nullptr;

        println("User database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
                custom_utils::COLOR::GREEN);
    }

    {
        performanceWatcher.start();
        println("Initializing virtual fs database", custom_utils::COLOR::GREEN);

        virtual_fs_db::virtual_fs *virtual_fs = new virtual_fs_db::virtual_fs();
        virtual_fs->initialize();
        delete virtual_fs;
        virtual_fs = nullptr;

        println("Virtual fs database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) +
                    "ms",
                custom_utils::COLOR::GREEN);
    }

    // launch FTPS server in worker thread
    std::thread ftps_server_thread([]() { ftps_server::server ftps_server = ftps_server::server(); });
    println("FTPS server worker thread started", custom_utils::COLOR::GREEN);

    // // std::thread sftp_server_thread([]() { sftp_server::server sftp_server = sftp_server::server(); });
    // // println("SFTP server worker thread started", custom_utils::COLOR::GREEN);

    // prevent main thread to die
    ftps_server_thread.join();
    // // sftp_server_thread.join();

    return 0;
}
