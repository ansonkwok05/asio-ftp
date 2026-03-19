#include "src/custom_utils.h"
#include "src/config/parser.h"
#include "src/database/sqlite_wrapper.h"
#include "src/database/user_db.h"
#include "src/database/virtual_fs_db.h"
#include "src/ftp/server.h"

#include <string>

using custom_utils::println;

config::parsed_config init_config()
{
    custom_utils::stopwatch performanceWatcher;
    performanceWatcher.start();

    config::parsed_config cfg = config::read_json_config();
    if (cfg.users.empty())
    {
        println("No users configured", custom_utils::COLOR::YELLOW);
    }

    println("Config loaded in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    return cfg;
}

void init_sql_db()
{
    custom_utils::stopwatch performanceWatcher;
    performanceWatcher.start();

    println("Initializing SQLite database", custom_utils::COLOR::GREEN);

    sqlite_wrapper::SQLiteDb *database = new sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING);
    database->init_db();
    delete database;
    database = nullptr;

    println("SQLite database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);
}

void init_user_table(config::parsed_config cfg)
{
    custom_utils::stopwatch performanceWatcher;
    performanceWatcher.start();

    println("Initializing user database", custom_utils::COLOR::GREEN);

    user_db::user *user = new user_db::user();
    user->initialize();

    for (const auto &[name, password] : cfg.users)
    {
        if (user->get_id_by_name(name).size() == 0)
        {
            // todo: if any new users detected,
            // clear old user table before using new users

            println("Attemping to create user \"" + name + "\"", custom_utils::COLOR::YELLOW);
            user->create_user(name, password);
            println("Created user \"" + name + "\"", custom_utils::COLOR::GREEN);
        }
    }

    delete user;
    user = nullptr;

    println("User database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);
}

void init_fs_table()
{
    custom_utils::stopwatch performanceWatcher;
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

int main()
{
    config::parsed_config cfg = init_config();

    std::srand(std::time(NULL));

    init_sql_db();
    init_user_table(cfg);
    init_fs_table();

    // start ftp server
    ftp::server ftp_server = ftp::server(cfg);

    return 0;
}
