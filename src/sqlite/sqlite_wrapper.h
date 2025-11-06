#pragma once

#include <sqlite3.h>

#include <string>
#include <vector>
#include <map>

namespace sqlite_wrapper
{
    constexpr bool ENABLE_LOGGING = true;

    class SQLiteDb
    {
    public:
        SQLiteDb();
        SQLiteDb(bool logging);

        ~SQLiteDb();

        void init_db(); // this calls all init functions at once

        void connect();

        /**
         * Inserts values from value_vector into columns from column_vector. Sizes of both vector must match.
         * @param table_name Name of the table.
         * @param column_vector Columns to insertion.
         * @param value_vector Values to insert.
         */
        bool insert_data(std::string table_name, std::vector<std::string> column_vector,
                         std::vector<std::string> value_vector);

        /**
         * Reads data from table.
         * @param table_name Name of the table
         * @param columns_vector Columns to read, leave empty to read all
         * @param returned_data Data returned from database
         */
        bool read_data(std::string table_name, std::vector<std::string> columns_vector,
                       std::vector<std::string> &returned_data);

        /**
         * Deletes a row in the table. No error is thrown if the row doesn't exist,
         * except when primary_key_column is not found.
         * @param table_name Name of the table
         * @param primary_key_column Primary key column name
         * @param value Primary key value for deletion
         * @return true if success, false if failed
         */
        bool delete_data(std::string table_name, std::string primary_key_column, std::string value);

    private:
        /**
         * TARGET_TABLES[TABLE_NAME] -> VECTOR of COLUMN DEFINITIONS
         */
        const std::map<std::string, std::vector<std::string>> TARGET_TABLES = {
            {
                "users",
                {
                    "user_id CHAR(36) PRIMARY KEY NOT NULL",
                    "username VARCHAR(30) UNIQUE NOT NULL",
                    "password VARCHAR(255) NOT NULL",
                },
            },
            {
                "files",
                {
                    "file_id CHAR(36) PRIMARY KEY NOT NULL",
                    "user_id CHAR(36) NOT NULL",
                    "FOREIGN KEY (user_id) REFERENCES users (user_id)",
                },
            },
            {
                "files_metadata",
                {
                    "file_name VARCHAR(255) NOT NULL",
                    "file_path VARCHAR(1024) NOT NULL",
                    "file_size BIGINT NOT NULL",
                    "modified_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP", // default is UTC time
                    "is_directory INTEGER NOT NULL",
                    "file_id CHAR(36) NOT NULL",
                    "FOREIGN KEY (file_id) REFERENCES files (file_id)",
                },
            },
        };

        const std::string OPTIMIZATIONS =
            "PRAGMA journal_mode = WAL; PRAGMA synchronous = normal; PRAGMA journal_size_limit = 6144000";

        bool allowLogging = false;

        struct SQLite_Context
        {
            int argc = 0;                       // number of columns
            std::vector<std::string> argv;      // results
            std::vector<std::string> azColName; // column names
        };
        sqlite3 *db;

        // initialization functions
        void check_data_folder_exists();
        void check_db_file_exists();
        void check_tables();
        void set_optimizations();

        void create_table(std::string table_name);
        void check_table_structure(std::string table_name);

        void println(std::string message);
        void println(std::string message, int color);

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