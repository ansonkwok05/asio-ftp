#include "sqlite_wrapper.h"
#include "customUtils.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using customUtils::print;
using customUtils::resetPrintColor;
using customUtils::setPrintColor;

namespace sqlite_wrapper
{
    SQLiteDb::SQLiteDb()
    {
        check_data_folder_exists();

        check_db_file_exists();

        check_table_count();

        check_existing_tables(); // this will create missing tables with predefined structure but will not delete table
    }

    void SQLiteDb::check_data_folder_exists()
    {
        // checking for data directory existance
        if (std::filesystem::is_directory("data"))
        {
            print("Found data folder\n");
            return;
        }
        setPrintColor("Yellow");
        print("data folder does not exist, creating a one right now\n");
        resetPrintColor();
        std::filesystem::create_directory("data");
    }

    void SQLiteDb::check_db_file_exists()
    {
        // checking for db file existance
        if (std::filesystem::exists("data/storage.db"))
        {
            print("Found data/storage.db\n");

            int rc = sqlite3_open("data/storage.db", &db);
            if (rc != SQLITE_OK || true)
            {
                throw std::runtime_error("Failed to open database.");
            }

            print("Opened database storage.db\n");
            return;
        }

        setPrintColor("Yellow");
        print("data/storage.db does not exist, creating a new one now\n");
        resetPrintColor();

        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create database.");
        }

        print("Created and opened storage.db at ./data\n");
    }

    void SQLiteDb::check_table_count()
    {
        // checking for table count

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            "SELECT count(*) FROM sqlite_master WHERE type='table';",
            sqlite_callback,
            &context,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            throw std::runtime_error(errMsg);
        }

        int tablesCount = context.argv[0][0] - '0';

        if (tablesCount > 3)
        {
            throw std::runtime_error("Unexpected table count (" + std::to_string(tablesCount) + "). Should not be more than 3.");
        }
        else if (tablesCount != 0)
        {

            print((int)tablesCount);
            print(" tables found in current database\n");
        }
        else
        {
            setPrintColor("Yellow");
            print("No tables found in database\n");
            resetPrintColor();
        }
    }

    void SQLiteDb::check_existing_tables()
    {
        // checking for all existing tables

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            "SELECT * FROM sqlite_master WHERE type='table';",
            sqlite_callback,
            &context,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            throw std::runtime_error(errMsg);
        }

        // tables that should exist in database
        const std::vector<std::string> target_tables = {"users", "files", "file_metadata"};

        if (context.argv.size() == 0)
        {
            for (int i = 0; i < target_tables.size(); i++)
            {
                createTable(target_tables[i]);
            }
            return; // early exit to create all tables
        }

        std::unordered_map<std::string, bool> existing_tables;
        for (int i = 0; i < context.argv.size(); i++)
        {
            existing_tables[context.argv[i]] = true;
        }

        print("Current tables:\n");
        for (const auto &[key, value] : existing_tables)
        {
            print(key + " ");
        }
        print("\n");

        // find untargeted tables
        for (const auto &[key, value] : existing_tables)
        {
            if (std::find(target_tables.begin(), target_tables.end(), key) == target_tables.end())
            {
                print("Unexpected table " + key + " found in database\n");
            }
        }

        // find targeted but missing tables
        for (int i = 0; i < target_tables.size(); i++)
        {
            if (!existing_tables[target_tables[i]])
            {
                print("Expected table " + target_tables[i] + " not found in database\n");
                createTable(target_tables[i]);
            }
        }
    }

    void SQLiteDb::createTable(std::string table_name)
    {
        print("Creating table " + table_name + "\n");

        const std::map<std::string, std::string> table_structure = {
            {"users", "(userid CHAR(36) PRIMARY KEY, username );"},
            {"files", ""},
            {"file_metadata", ""}};

        if (table_structure.find(table_name) == table_structure.end())
        {
            throw std::runtime_error("Unknown table name / structure.");
        }

        std::string table_creation_query = "CREATE TABLE IF NOT EXISTS";

        table_creation_query += " " + table_name;
        table_creation_query += " " + table_structure.at(table_name);

        // SQLite_Context context; // context for return values
        // char * errMsg;          // returned error message
        // int rc = sqlite3_exec(
        //     db,
        //     "CREATE TABLE IF NOT EXISTS",
        //     sqlite_callback,
        //     &context,
        //     &errMsg
        // );
        // if (rc != SQLITE_OK) {
        //     print("Failed to create tables.\n");
        //     throw std::runtime_error(errMsg);
        // }
    }

    int SQLiteDb::sqlite_callback(void *context, int argc, char **argv, char **azColName)
    {
        setPrintColor("Blue");
        print("SQLITEDB CALLBACK\n");
        resetPrintColor();

        // using a context to return values
        SQLite_Context *c = (SQLite_Context *)context;
        if (!c)
            throw std::runtime_error("Query context error.");

        // copy argc
        c->argc += argc;

        // copy argv and azColName
        for (int i = 0; i < argc; i++)
        {
            c->argv.push_back(argv[i]);
            c->azColName.push_back(azColName[i]);
        }
        return 0;
    }

    void SQLiteDb::print_table(SQLite_Context context)
    {
        print("\nPRINT TABLE:\n");

        print("argc = " + std::to_string(context.argc) + "\n");
        print("argv.size() = " + std::to_string(context.argv.size()) + "\n");
        print("azColName.size() = " + std::to_string(context.azColName.size()) + "\n");
        print("\n---\n");

        for (int i = 0; i < context.argv.size(); i++)
        {
            for (int j = 0; j < context.argc; j++)
            {
                print("Column " + context.azColName[j] + "\n");
                print("Data   " + context.argv[i * j + j] + "\n");
            }
        }
        print("---\n:PRINT TABLE\n\n");
    }

    SQLiteDb::~SQLiteDb()
    {
        sqlite3_close(db);
    }
}