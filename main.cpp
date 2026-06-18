#include "src/custom_utils.hpp"
#include "src/config/parser.hpp"
#include "src/database/fs_handler.hpp"
#include "src/database/sqlite_wrapper.hpp"
#include "src/database/users.hpp"
#include "src/database/fs_objects.hpp"
#include "src/ftp/server.hpp"

#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>

using custom_utils::println;

config::parsed_config init_config()
{
    config::parsed_config cfg = config::read_json_config();
    if (cfg.users.empty())
    {
        println("No user configured", custom_utils::COLOR::YELLOW);
    }

    return cfg;
}

void init_sql_db()
{
    sqlite_wrapper::SQLiteDb().init_db();
}

bool any_users_changed(std::map<std::string, std::string> db_users, std::map<std::string, std::string> config_users)
{
    // using std::map so data is ordered
    for (const auto &[db_user_name, db_user_password] : db_users)
    {
        // find if any side has missing users
        if (config_users.find(db_user_name) == config_users.end())
        {
            return true;
        }

        // check if any password changed
        if (config_users[db_user_name] != db_user_password)
        {
            return true;
        }
    }

    return false;
}

void init_user_table(config::parsed_config cfg)
{
    std::map<std::string, std::string> db_users;
    {
        users::users users_table = users::users();
        users_table.initialize();

        std::vector<users::user> all_user_credential = users_table.get_all_user_credentials();
        for (const users::user &user : all_user_credential)
        {
            db_users[user.name] = user.password;
        }
    }

    if (any_users_changed(db_users, cfg.users))
    {
        // new users config detected
        println("Users config changed. Backing up storage.db and creating a new one", custom_utils::COLOR::YELLOW);

        int i = 1;
        while (fs_handler::file_exists("data/old_storage" + std::to_string(i) + ".db"))
        {
            i++;
        }
        fs_handler::rename_file("data/storage.db", "data/old_storage" + std::to_string(i) + ".db");

        // create a new storage.db
        init_sql_db();
    }

    users::users users_table = users::users();
    users_table.initialize();

    for (const auto &[name, password] : cfg.users)
    {
        users_table.create_user(name, password);
        println("Created user \"" + name + "\"", custom_utils::COLOR::GREEN);
    }
}

void init_fs_objects_table()
{
    fs_objects::fs_objects().initialize();
}

int main()
{
    std::srand(std::time(NULL));

    custom_utils::stopwatch performanceWatcher;
    performanceWatcher.start();

    config::parsed_config cfg = init_config();
    println("Config loaded in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    init_sql_db();
    println("SQLite database initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    init_user_table(cfg);
    println("User table initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    init_fs_objects_table();
    println("Virtual_fs table initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    // start ftp server
    ftp::server ftp_server = ftp::server(cfg);

    return 0;
}
