#include "sqlite_wrapper.h"
#include "fs_handler.h"
#include "../custom_utils.h"

#include <stdexcept>
#include <algorithm>
#include <string>

namespace sqlite_wrapper
{
    SQLiteDb::SQLiteDb()
    {
    }

    SQLiteDb::SQLiteDb(bool logging)
    {
        allowLogging = logging;
    }

    SQLiteDb::~SQLiteDb()
    {
        sqlite3_close(db);
    }

    void SQLiteDb::init_db()
    {
        check_data_folder_exists();

        check_db_file_exists();

        check_tables(); // check current and create missing tables with predefined structure

        set_optimizations(); // optimizations done to improve performance
    }

    void SQLiteDb::connect()
    {
        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            println("Failed to open database", "red");
            throw std::runtime_error("sqlite3_open operation failed");
        }

        println("Opened database storage.db", "green");
    }

    bool SQLiteDb::insert_data(std::string table_name, std::vector<std::string> column_vector,
                               std::vector<std::string> value_vector)
    {
        if (column_vector.size() != value_vector.size())
        {
            return false;
        }

        std::string data_insertion_query = "INSERT INTO " + table_name + " (";

        for (int i = 0; i < column_vector.size(); i++)
        {
            data_insertion_query += column_vector[i];
            if (i != column_vector.size() - 1)
            {
                data_insertion_query += ", ";
            }
        }
        data_insertion_query += ") VALUES ('";

        for (int i = 0; i < value_vector.size(); i++)
        {
            // check if value contains ', replace with '' to insert correctly
            value_vector.at(i) = custom_utils::replaceString(value_vector.at(i), "'", "''");

            data_insertion_query += value_vector[i];
            if (i != value_vector.size() - 1)
            {
                data_insertion_query += "', '";
            }
        }
        data_insertion_query += "')";

        char *errMsg; // returned error message
        int rc = sqlite3_exec(db, data_insertion_query.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            // could be due to full/corrupted database, or incorrect data/data type is inserted
            println("Failed to insert data", "red");
            println("Query: " + data_insertion_query, "red");
            println("Error code -> " + std::to_string(rc), "red");
            println("Error msg -> " + std::string(errMsg), "red");

            return false;
        }
        return true;
    }

    bool SQLiteDb::read_data(std::string table_name, std::vector<std::string> columns_vector,
                             std::vector<std::string> &returned_data)
    {
        std::string data_selection_query = "SELECT ";

        if (columns_vector.size() == 0)
        {
            data_selection_query += "*";
        }
        else
        {
            for (int i = 0; i < columns_vector.size(); i++)
            {
                data_selection_query += columns_vector[i];
                if (i != columns_vector.size() - 1)
                {
                    data_selection_query += ", ";
                }
            }
        }

        data_selection_query += " FROM " + table_name;

        SQLite_Context context;
        char *errMsg; // returned error message
        int rc = sqlite3_exec(db, data_selection_query.c_str(), sqlite_callback, &context, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to read data", "red");
            println("Query: " + data_selection_query, "red");
            println("Error code -> " + std::to_string(rc) + "\n", "red");
            println("Error msg -> " + std::string(errMsg) + "\n", "red");

            return false;
        }

        for (auto str : context.argv)
        {
            returned_data.push_back(str);
        }
        return true;
    }

    bool SQLiteDb::delete_data(std::string table_name, std::string primary_key_column, std::string value)
    {
        std::string data_deletion_query =
            "DELETE FROM " + table_name + " WHERE " + primary_key_column + " = '" + value + "'";

        char *errMsg; // returned error message
        int rc = sqlite3_exec(db, data_deletion_query.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to delete data", "red");
            println("Query: " + data_deletion_query, "red");
            println("Error code -> " + std::to_string(rc), "red");
            println("Error msg -> " + std::string(errMsg), "red");

            return false;
        }
        return true;
    }

    void SQLiteDb::check_data_folder_exists()
    {
        // checking for data directory existance
        if (fs_handler::directory_exists("data"))
        {
            println("Found data folder", "green");
            return;
        }

        println("data folder does not exist, creating a new one right now", "yellow");
        fs_handler::create_directory("data");

        if (!fs_handler::directory_exists("data"))
        {
            println("Unable to create data folder", "red");
            throw std::runtime_error("create_directory operation failed");
        }
        println("Created ./data", "green");
    }

    void SQLiteDb::check_db_file_exists()
    {
        // checking for db file existance, and connect if found
        if (fs_handler::file_exists("data/storage.db"))
        {
            println("Found data/storage.db", "green");

            int rc = sqlite3_open("data/storage.db", &db);
            if (rc != SQLITE_OK)
            {
                println("Failed to create database", "red");
                throw std::runtime_error("sqlite3_open operation failed");
            }
            println("Connected to data/storage.db", "green");
            return;
        }

        println("data/storage.db does not exist, creating a new one now", "yellow");

        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            println("Failed to create database", "red");
            throw std::runtime_error("sqlite3_open operation failed");
        }

        println("Created and connected to data/storage.db", "green");
    }

    void SQLiteDb::check_tables()
    {
        // checking for all existing tables

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc =
            sqlite3_exec(db, "SELECT name FROM sqlite_master WHERE type='table'", sqlite_callback, &context, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to list all table names", "red");
            throw std::runtime_error(errMsg);
        }

        // if no tables exists, create em all
        if (context.argv.size() == 0)
        {
            println("No tables found in database", "yellow");
            for (auto const &[key, val] : TARGET_TABLES)
            {
                create_table(key);
            }
            check_tables(); // double check tables again after creating em all
            return;
        }

        // if table count matches, check if all table matches
        if (context.argv.size() == TARGET_TABLES.size())
        {
            bool allTableNamesValid = true;

            // for each table name, search through target table list for a match
            for (int i = 0; i < context.argv.size(); i++)
            {
                if (TARGET_TABLES.find(context.argv[i]) ==
                    TARGET_TABLES.end()) // an unknown table name is found in database
                {
                    allTableNamesValid = false;
                    println("Unexpected table \"" + context.argv[i] + "\" found in database", "red");
                    throw std::runtime_error("possible different database version");
                }
            }

            if (allTableNamesValid)
            {
                for (int i = 0; i < context.argv.size(); i++)
                {
                    check_table_structure(context.argv[i]);
                }
                println("All tables are validated in database", "green");
                return; // table is valid
            }
        }

        // find targeted but missing tables
        for (auto const &[key, val] : TARGET_TABLES)
        {
            if (std::find(context.argv.begin(), context.argv.end(), key) == context.argv.end())
            {
                println("Missing table \"" + key + "\"", "yellow");
                create_table(key);
            }
        }

        check_tables(); // double check to confirm correct tables
    }

    void SQLiteDb::set_optimizations()
    {
        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(db, OPTIMIZATIONS.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to list all table names", "red");
            throw std::runtime_error(errMsg);
        }
        println("Database optimizations set", "green");
    }

    void SQLiteDb::create_table(std::string table_name)
    {
        println("Creating \"" + table_name + "\" table", "yellow");

        if (TARGET_TABLES.find(table_name) == TARGET_TABLES.end())
        {
            println("Cannot create table, unknown table name", "red");
            throw std::runtime_error("create_table operation failed");
        }

        std::string table_creation_query = "CREATE TABLE IF NOT EXISTS " + table_name + " (";
        for (int i = 0; i < TARGET_TABLES.at(table_name).size(); i++)
        {
            table_creation_query += TARGET_TABLES.at(table_name)[i];
            if (i != TARGET_TABLES.at(table_name).size() - 1)
            {
                table_creation_query += ", ";
            }
        }
        table_creation_query += ")";

        println(table_creation_query, "blue");

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(db, table_creation_query.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to create \"" + table_name + "\" table.", "red");
            throw std::runtime_error(errMsg);
        }
    }

    void SQLiteDb::check_table_structure(std::string table_name)
    {
        std::string table_structure_check_query = "SELECT sql FROM sqlite_master WHERE name = '" + table_name + "'";

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(db, table_structure_check_query.c_str(), sqlite_callback, &context, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to check table structure", "red");
            throw std::runtime_error(errMsg);
        }

        if (context.argv.size() != 1)
        {
            println("Unexpected return value from SQLITE, table structure check must return size of 1", "red");
            throw std::runtime_error("more than one table with the same name??");
        }

        std::string table_creation_query = "CREATE TABLE " + table_name + " (";
        for (int i = 0; i < TARGET_TABLES.at(table_name).size(); i++)
        {
            table_creation_query += TARGET_TABLES.at(table_name)[i];
            if (i != TARGET_TABLES.at(table_name).size() - 1)
            {
                table_creation_query += ", ";
            }
        }
        table_creation_query += ")";

        if (context.argv[0] != table_creation_query)
        {
            // the method used to create this table is different
            println("Detected a different table structure in \"" + table_name + "\" table", "red");
            throw std::runtime_error("possible different table version");
        }
    }

    void SQLiteDb::println(std::string message)
    {
        if (!allowLogging)
            return;
        custom_utils::println(message);
    }

    void SQLiteDb::println(std::string message, std::string color)
    {
        if (!allowLogging)
            return;
        custom_utils::println(message, color);
    }

    int SQLiteDb::sqlite_callback(void *context, int argc, char **argv, char **azColName)
    {
        // using a context to return values
        SQLite_Context *c = (SQLite_Context *)context;
        if (!c)
        {
            custom_utils::println("Query context error", "red");
            throw std::runtime_error("sqlite_callback operation failed");
        }

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
} // namespace sqlite_wrapper