#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

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
    void check_existing_tables();
    void createTable(std::string);

    static int sqlite_callback(void *, int, char **, char **);
    void print_table(SQLite_Context);
};