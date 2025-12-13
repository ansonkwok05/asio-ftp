#pragma once

#include <sqlite3.h>

#include "../custom_utils.h"

#include <string>
#include <vector>
#include <map>

namespace sqlite_wrapper
{
    constexpr bool ENABLE_LOGGING = true;

    constexpr char OPTIMIZATIONS[] = "PRAGMA journal_mode = WAL; PRAGMA synchronous = normal; PRAGMA "
                                     "journal_size_limit = 67108864; PRAGMA mmap_size = 134217728; PRAGMA cache_size = "
                                     "2000; PRAGMA busy_timeout = 5000;";

    struct SQLite_Context
    {
        int argc = 0;                       // number of columns
        std::vector<std::string> argv;      // results
        std::vector<std::string> azColName; // column names
    };

    class SQLiteDb
    {
    public:
        SQLiteDb();

        /**
         * Set to "true" to enable logging.
         * Default: "false".
         */
        SQLiteDb(bool logging);

        ~SQLiteDb();

        /**
         * Initialize the Sqlite database
         */
        void init_db();

        /**
         * Start the connection to the Sqlite database
         */
        void connect();

        /**
         * Runs a sql query
         * returns the result of the sql query
         * @param sql_query sql query string
         * @return result as vector of string
         */
        std::vector<std::string> run_query(std::string sql_query);

        /**
         * Runs a parameterized sql query
         * returns the result of the parameterized sql query
         * @param sql_query sql query string with "?"
         * @param params parameters
         * @return result as vector of string
         */
        std::vector<std::string> run_param_query(std::string sql_query, std::vector<std::string> params);

    private:
        bool allowLogging = false;

        sqlite3 *db;

        // initialization functions
        void check_data_folder_exists();
        void check_db_file_exists();
        void set_optimizations();

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);

        /**
         * A callback that copys return value into a SQLite_Context
         * @param context A SQLite_Context that return values will be copied into
         * @param argc
         * @param argv
         * @param azColName
         */
        static int sqlite_callback(void *context, int argc, char **argv, char **azColName);
    };
} // namespace sqlite_wrapper