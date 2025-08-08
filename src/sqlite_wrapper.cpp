#include "sqlite_wrapper.h"
#include "custom_utils.h"
#include "fs_handler.h"

#include <stdexcept>
#include <algorithm>
#include <string>

namespace sqlite_wrapper
{
    using custom_utils::print;

    SQLiteDb::SQLiteDb()
    {
        check_data_folder_exists();

        check_db_file_exists();

        check_tables(); // check current and create missing tables with predefined structure

        set_optimizations(); // optimizations done to improve performance
    }

    SQLiteDb::~SQLiteDb()
    {
        sqlite3_close(db);
    }

    /**
     * No safe guard here, do validate data before inserting data. Beware of wrong data type.
     * @param table_name Name of the table.
     * @param column_vector Columns to insertion.
     * @param value_vector Values to insert.
     */
    void SQLiteDb::insert_data(std::string table_name, std::vector<std::string> column_vector, std::vector<std::string> value_vector)
    {
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
            data_insertion_query += value_vector[i];
            if (i != value_vector.size() - 1)
            {
                data_insertion_query += "', '";
            }
        }
        data_insertion_query += "')";

        char *errMsg; // returned error message
        int rc = sqlite3_exec(
            db,
            data_insertion_query.c_str(),
            0,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to insert data\n", "red");
            print("Query: " + data_insertion_query + "\n", "red");
            throw std::runtime_error(errMsg); // could be due to full / corrupted database, or incorrect data type is inserted
        }
    }

    /**
     * Reads data from table.
     * @param table_name Name of the table
     * @param columns_vector Columns to read, leave empty to read all
     */
    void SQLiteDb::read_data(std::string table_name, std::vector<std::string> columns_vector)
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

        char *errMsg; // returned error message
        int rc = sqlite3_exec(
            db,
            data_selection_query.c_str(),
            0,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to read data\n", "red");
            print("Query: " + data_selection_query + "\n", "red");
            throw std::runtime_error(errMsg);
        }
    }

    /**
     * Deletes a row in the table. No error is thrown if the row doesn't exist,
     * except when primary_key_column is not found.
     * @param table_name Name of the table
     * @param primary_key_column Primary key column name
     * @param value Primary key value for deletion
     */
    void SQLiteDb::delete_data(std::string table_name, std::string primary_key_column, std::string value)
    {
        std::string data_deletion_query = "DELETE FROM " + table_name + " WHERE " + primary_key_column + " = '" + value + "'";

        char *errMsg; // returned error message
        int rc = sqlite3_exec(
            db,
            data_deletion_query.c_str(),
            0,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to delete data\n", "red");
            print("Query: " + data_deletion_query + "\n", "red");
            throw std::runtime_error(errMsg); // something is really wrong when this throws, check primary_key_column arg
        }
    }

    void SQLiteDb::check_data_folder_exists()
    {
        // checking for data directory existance
        if (fs_handler::directory_exists("data"))
        {
            print("Found data folder\n", "green");
            return;
        }

        print("data folder does not exist, creating a one right now\n", "yellow");
        fs_handler::create_directory("data");

        if (!fs_handler::directory_exists("data"))
        {
            print("\n", "red");
            throw std::runtime_error("Unable to create data folder");
        }
        print("Created ./data\n", "green");
    }

    void SQLiteDb::check_db_file_exists()
    {
        // checking for db file existance
        if (fs_handler::file_exists("data/storage.db"))
        {
            print("Found data/storage.db\n", "green");

            int rc = sqlite3_open("data/storage.db", &db);
            if (rc != SQLITE_OK)
            {
                print("\n", "red");
                throw std::runtime_error("Failed to open database.");
            }

            print("Opened database storage.db\n", "green");
            return;
        }

        print("data/storage.db does not exist, creating a new one now\n", "yellow");

        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            print("\n", "red");
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
            "SELECT name FROM sqlite_master WHERE type='table'",
            sqlite_callback,
            &context,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to list all table names\n", "red");
            throw std::runtime_error(errMsg);
        }

        // if no tables exists, create em all
        if (context.argv.size() == 0)
        {
            print("No tables found in database\n", "yellow");
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
                if (TARGET_TABLES.find(context.argv[i]) == TARGET_TABLES.end()) // an unknown table name is found in database
                {
                    allTableNamesValid = false;
                    print("\n", "red");
                    throw std::runtime_error("Unexpected table \"" + context.argv[i] + "\" found in database\n");
                }
            }

            if (allTableNamesValid)
            {
                for (int i = 0; i < context.argv.size(); i++)
                {
                    check_table_structure(context.argv[i]);
                }
                print("All tables are valid in database\n", "green");
                return; // table is valid
            }
        }

        // find targeted but missing tables
        for (auto const &[key, val] : TARGET_TABLES)
        {
            if (std::find(context.argv.begin(), context.argv.end(), key) == context.argv.end())
            {
                print("Missing table \"" + key + "\"\n", "yellow");
                create_table(key);
            }
        }

        check_tables(); // double check to confirm correct tables
    }

    void SQLiteDb::set_optimizations()
    {
        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            OPTIMIZATIONS.c_str(),
            0,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to list all table names\n", "red");
            throw std::runtime_error(errMsg);
        }
    }

    void SQLiteDb::create_table(std::string table_name)
    {
        print("Creating \"" + table_name + "\" table\n", "yellow");

        if (TARGET_TABLES.find(table_name) == TARGET_TABLES.end())
        {
            print("\n", "red");
            throw std::runtime_error("Cannot create table, unknown table name");
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

        print(table_creation_query + "\n", "blue");

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            table_creation_query.c_str(),
            0,
            0,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to create \"" + table_name + "\" table.\n", "red");
            throw std::runtime_error(errMsg);
        }
    }

    void SQLiteDb::check_table_structure(std::string table_name)
    {
        std::string table_structure_check_query = "SELECT sql FROM sqlite_master WHERE name = '" + table_name + "'";

        SQLite_Context context; // context for return values
        char *errMsg;           // returned error message
        int rc = sqlite3_exec(
            db,
            table_structure_check_query.c_str(),
            sqlite_callback,
            &context,
            &errMsg);
        if (rc != SQLITE_OK)
        {
            print("\nFailed to check table structure\n", "red");
            throw std::runtime_error(errMsg);
        }

        if (context.argv.size() != 1)
        {
            print("\n", "red");
            throw std::runtime_error("Unexpected return value from SQLITE, table structure check must return size of 1");
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
            print("\n", "red");
            throw std::runtime_error("Detected a different table structure in \"" + table_name + "\" table");
        }
    }

    int SQLiteDb::sqlite_callback(void *context, int argc, char **argv, char **azColName)
    {
        // using a context to return values
        SQLite_Context *c = (SQLite_Context *)context;
        if (!c)
        {
            print("\n", "red");
            throw std::runtime_error("Query context error.");
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
}