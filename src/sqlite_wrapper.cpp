#include "sqlite_wrapper.h"
#include "custom_utils.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

using custom_utils::print;

namespace sqlite_wrapper
{
    SQLiteDb::SQLiteDb()
    {
        check_data_folder_exists();

        check_db_file_exists();

        check_tables(); // check current and create missing tables with predefined structure
    }

    SQLiteDb::~SQLiteDb()
    {
        sqlite3_close(db);
    }

    void SQLiteDb::insert_data(std::string, int)
    {
        // todo: implement inserting data to table
    }

    void SQLiteDb::check_data_folder_exists()
    {
        // checking for data directory existance
        if (std::filesystem::is_directory("data"))
        {
            print("Found data folder\n", "green");
            return;
        }
        print("data folder does not exist, creating a one right now\n", "yellow");
        std::filesystem::create_directory("data");
    }

    void SQLiteDb::check_db_file_exists()
    {
        // checking for db file existance
        if (std::filesystem::exists("data/storage.db"))
        {
            print("Found data/storage.db\n", "green");

            int rc = sqlite3_open("data/storage.db", &db);
            if (rc != SQLITE_OK)
            {
                throw std::runtime_error("Failed to open database.");
            }

            print("Opened database storage.db\n", "green");
            return;
        }

        print("data/storage.db does not exist, creating a new one now\n", "yellow");

        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create database.");
        }

        print("Created and opened storage.db at ./data\n", "green");
    }

    void SQLiteDb::check_tables()
    {
        // checking for all existing tables

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            "SELECT name FROM sqlite_master WHERE type='table';",
            sqlite_callback,
            &context,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            throw std::runtime_error(errMsg);
        }

        // if no tables exists, create em all
        if (context.argv.size() == 0)
        {
            print("No tables found in database\n", "yellow");
            for (int i = 0; i < TARGET_TABLES.size(); i++)
            {
                create_table(TARGET_TABLES[i]);
            }
            check_tables(); // double check tables again after creating em all
            return;
        }

        // if table count matches, check if all table matches
        if (context.argv.size() == TARGET_TABLES.size())
        {
            bool allTablesValid = true;

            // for each targetted table name, search through current table list for a match
            for (int i = 0; i < TARGET_TABLES.size(); i++)
            {
                // if any no match, the current table is not valid
                if (std::find(context.argv.begin(), context.argv.end(), TARGET_TABLES[i]) == context.argv.end())
                {
                    allTablesValid = false;
                    break;
                }
            }

            if (allTablesValid)
            {
                print("All tables are valid in database\n", "green");
                return; // table is valid
            }
        }

        print("Mismatch tables in database\n", "yellow");

        print("Current tables:\n");
        for (int i = 0; i < context.argv.size(); i++)
        {
            // find untargeted tables, check if it doesnt exists in target_tables
            if (std::find(TARGET_TABLES.begin(), TARGET_TABLES.end(), context.argv[i]) == TARGET_TABLES.end())
            {
                custom_utils::setPrintColor("red");
                throw std::runtime_error("Unexpected table \"" + context.argv[i] + "\" found in database\n");
            }
            print("-> " + context.argv[i] + "\n");
        }
        print("\n");

        // find targeted but missing tables
        for (int i = 0; i < TARGET_TABLES.size(); i++)
        {
            if (std::find(context.argv.begin(), context.argv.end(), TARGET_TABLES[i]) == context.argv.end())
            {
                print("Expected table " + TARGET_TABLES[i] + " is not found in database\n");
                create_table(TARGET_TABLES[i]);
            }
        }

        check_tables(); // double check to confirm correct tables
    }

    void SQLiteDb::create_table(std::string table_name)
    {
        print("Creating \"" + table_name + "\" table\n");

        if (TABLE_STRUCTURES.find(table_name) == TABLE_STRUCTURES.end())
        {
            throw std::runtime_error("Unknown table name / structure.");
        }

        std::string table_creation_query = "CREATE TABLE IF NOT EXISTS";

        table_creation_query += " " + table_name;
        table_creation_query += " " + TABLE_STRUCTURES.at(table_name);

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            table_creation_query.c_str(),
            NULL,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("Failed to create \"" + table_name + "\" table.\n", "red");
            throw std::runtime_error(errMsg);
        }
    }

    int SQLiteDb::sqlite_callback(void *context, int argc, char **argv, char **azColName)
    {
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
}