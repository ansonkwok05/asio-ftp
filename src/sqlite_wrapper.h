#pragma once

#include <sqlite3.h>

#include <string>
#include <vector>
#include <map>

namespace sqlite_wrapper
{
    class SQLiteDb
    {
    public:
        SQLiteDb();
        ~SQLiteDb();
        void insert_data(std::string, std::vector<std::string>, std::vector<std::string>);

    private:
        // TARGET_TABLES(MAP of STRINGS) -> COLUMN DEFINITIONS(VECTOR of STRINGS)
        // todo: files_meta need more data columns, should include filename, mimetype, filesize, upload time, creation time, modify time
        const std::map<std::string, std::vector<std::string>> TARGET_TABLES = {
            {"users", {"userid CHAR(36) PRIMARY KEY NOT NULL", "username VARCHAR(30) NOT NULL", "password VARCHAR(255) NOT NULL", "email VARCHAR(255)"}}, {"files", {"fileid CHAR(36) PRIMARY KEY NOT NULL", "userid CHAR(36) NOT NULL", "FOREIGN KEY (userid) REFERENCES users (userid)"}}, {"files_metadata", {"fileid CHAR(36) NOT NULL", "FOREIGN KEY (fileid) REFERENCES files(fileid)"}}};

        const std::string OPTIMIZATIONS = "PRAGMA journal_mode = WAL; PRAGMA synchronous = normal; PRAGMA journal_size_limit = 6144000";

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

        void create_table(std::string);
        void check_table_structure(std::string);

        static int sqlite_callback(void *, int, char **, char **);
        void print_table(SQLite_Context);
    };
}