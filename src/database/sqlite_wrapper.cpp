#include "sqlite_wrapper.h"
#include "fs_handler.h"
#include "../custom_utils.h"

#include <sqlite3.h>

#include <stdexcept>
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
    }

    void SQLiteDb::connect()
    {
        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            println("Failed to open database", custom_utils::COLOR::RED);
            throw std::runtime_error("sqlite3_open operation failed");
        }

        println("Opened database storage.db", custom_utils::COLOR::GREEN);

        // database optimizations for performance improvement
        set_optimizations();
    }

    std::vector<std::string> SQLiteDb::run_query(std::string sql_query)
    {
        SQLite_Context context;
        char *errMsg;

        int rc = sqlite3_exec(db, sql_query.c_str(), sqlite_callback, &context, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to execute query", custom_utils::COLOR::RED);
            println("Query: " + sql_query, custom_utils::COLOR::RED);
            println("Error code -> " + std::to_string(rc), custom_utils::COLOR::RED);
            println("Error msg -> " + std::string(errMsg), custom_utils::COLOR::RED);

            throw std::runtime_error(errMsg);
        }

        return context.argv;
    }

    std::vector<std::string> SQLiteDb::run_param_query(std::string sql_query, std::vector<std::string> params)
    {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db, sql_query.c_str(), sql_query.length(), &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            println("Failed to prepare query", custom_utils::COLOR::RED);
            println("Query: " + sql_query, custom_utils::COLOR::RED);
            println("Error code -> " + std::to_string(rc), custom_utils::COLOR::RED);

            throw std::runtime_error("Failed to call run_param_query");
        }

        for (int i = 0; i < params.size(); i++)
        {
            rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), params[i].length(), SQLITE_STATIC);
            if (rc != SQLITE_OK)
            {
                println("Failed to bind param", custom_utils::COLOR::RED);
                println("Param: " + params[i], custom_utils::COLOR::RED);
                println("Param index: " + std::to_string(i), custom_utils::COLOR::RED);
                println("Error code -> " + std::to_string(rc), custom_utils::COLOR::RED);

                throw std::runtime_error("Failed to bind param");
            }
        }

        std::vector<std::string> result = {};

        // start stepping through the rows
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            sqlite3_column_text(stmt, 1);
            for (int i = 0, max = sqlite3_column_count(stmt); i < max; i++)
            {
                result.push_back((const char *)sqlite3_column_text(stmt, i));
            }
        }

        if (rc == SQLITE_MISUSE)
        {
            println("Library misuse", custom_utils::COLOR::RED);
            println("Query: " + sql_query, custom_utils::COLOR::RED);
            println("Param size: " + std::to_string(params.size()), custom_utils::COLOR::RED);
            println("Error code -> " + std::to_string(rc), custom_utils::COLOR::RED);

            throw std::runtime_error("Failed to step through sql query");
        }

        // finish by deconstruction
        sqlite3_finalize(stmt);

        return result;
    }

    void SQLiteDb::check_data_folder_exists()
    {
        // checking for data directory existance
        if (fs_handler::directory_exists("data"))
        {
            println("Found data folder", custom_utils::COLOR::GREEN);
            return;
        }

        println("data folder does not exist, creating a new one right now", custom_utils::COLOR::YELLOW);
        fs_handler::create_directory("data");

        if (!fs_handler::directory_exists("data"))
        {
            println("Unable to create data folder", custom_utils::COLOR::RED);
            throw std::runtime_error("create_directory operation failed");
        }
        println("Created ./data", custom_utils::COLOR::GREEN);
    }

    void SQLiteDb::check_db_file_exists()
    {
        // checking for db file existance, and connect if found
        if (fs_handler::file_exists("data/storage.db"))
        {
            println("Found data/storage.db", custom_utils::COLOR::GREEN);

            int rc = sqlite3_open("data/storage.db", &db);
            if (rc != SQLITE_OK)
            {
                println("Failed to create database", custom_utils::COLOR::RED);
                throw std::runtime_error("sqlite3_open operation failed");
            }
            println("Connected to data/storage.db", custom_utils::COLOR::GREEN);
            return;
        }

        println("data/storage.db does not exist, creating a new one now", custom_utils::COLOR::YELLOW);

        int rc = sqlite3_open("data/storage.db", &db);
        if (rc != SQLITE_OK)
        {
            println("Failed to create database", custom_utils::COLOR::RED);
            throw std::runtime_error("sqlite3_open operation failed");
        }

        println("Created and connected to data/storage.db", custom_utils::COLOR::GREEN);
    }

    void SQLiteDb::set_optimizations()
    {
        SQLite_Context context;
        char *errMsg;

        int rc = sqlite3_exec(db, OPTIMIZATIONS, 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            println("Failed to list all table names", custom_utils::COLOR::RED);
            throw std::runtime_error(errMsg);
        }
        println("Database optimizations set", custom_utils::COLOR::GREEN);
    }

    void SQLiteDb::println(std::string message)
    {
        if (!allowLogging)
            return;
        custom_utils::println(message);
    }

    void SQLiteDb::println(std::string message, custom_utils::COLOR color)
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
            custom_utils::println("Query context error", custom_utils::COLOR::RED);
            throw std::runtime_error("sqlite_callback operation failed");
        }

        // push argc
        c->argc += argc;

        // push argv and azColName
        for (int i = 0; i < argc; i++)
        {
            c->argv.push_back(argv[i]);
            c->azColName.push_back(azColName[i]);
        }
        return 0;
    }
} // namespace sqlite_wrapper