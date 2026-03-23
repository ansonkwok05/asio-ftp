#include "src/custom_utils.h"
#include "src/config/parser.h"
#include "src/database/fs_handler.h"
#include "src/database/sqlite_wrapper.h"
#include "src/database/user_db.h"
#include "src/database/virtual_fs_db.h"
#include "src/ftp/server.h"

#include <boost/json/stream_parser.hpp>
#include <string>
#include <map>

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

void init_sql_db(bool show_logs = true)
{
    if (show_logs)
        sqlite_wrapper::SQLiteDb(sqlite_wrapper::ENABLE_LOGGING).init_db();
    else
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
        user_db::user user = user_db::user();
        user.initialize();

        std::vector<std::string> temp_userlist = user.get_username_list();
        int i = 0;
        while (i < temp_userlist.size())
        {
            db_users[temp_userlist[i]] = temp_userlist[i + 1];
            i += 2;
        }
    }

    if (any_users_changed(db_users, cfg.users))
    {
        // new users config detected
        println("Users config changed. Backing up storage.db and creating a new one", custom_utils::COLOR::YELLOW);
        fs_handler::rename_file("data/storage.db", "data/backup_storage.db");

        // create a new storage.db
        init_sql_db(false);
    }

    user_db::user user = user_db::user();
    user.initialize();
    for (const auto &[name, password] : cfg.users)
    {
        user.create_user(name, password);
        println("Created user \"" + name + "\"", custom_utils::COLOR::GREEN);
    }
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

    virtual_fs_db::virtual_fs().initialize();
    println("Virtual_fs table initialization done in " + std::to_string(performanceWatcher.lapNs() / 1'000'000) + "ms",
            custom_utils::COLOR::GREEN);

    // start ftp server
    ftp::server ftp_server = ftp::server(cfg);

    return 0;
}
