#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>

namespace sqlite_wrapper
{
    const std::vector<std::string> TARGET_TABLES = {"users", "files", "file_metadata"};
    const std::map<std::string, std::string> TABLE_STRUCTURES = {
        {"users", "(userid CHAR(36) PRIMARY KEY, username VARCHAR(30), password VARCHAR(255), email VARCHAR(255));"},
        {"files", "(fileid CHAR(36) PRIMARY KEY, userid CHAR(36), FOREIGN KEY (userid) REFERENCES users (userid));"},
        {"file_metadata", "(fileid CHAR(36), FOREIGN KEY (fileid) REFERENCES files(fileid));"}}; // incomplete data types, should include filename, mimetype, filesize, upload time, creation time, modify time

    class SQLiteDb
    {
    public:
        SQLiteDb();
        ~SQLiteDb();

    private:
        struct SQLite_Context
        {
            int argc = 0;                       // number of columns
            std::vector<std::string> argv;      // results
            std::vector<std::string> azColName; // column names
        };
        sqlite3 *db;

        void check_data_folder_exists();
        void check_db_file_exists();
        void check_table_count();
        void check_tables();

        void create_table(std::string);

        static int sqlite_callback(void *, int, char **, char **);
        void print_table(SQLite_Context);
    };
}