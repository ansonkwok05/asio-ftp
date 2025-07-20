#pragma once

#include <sqlite3.h>
#include <string>

struct SQLite_TableCountContext {
    int tcount;
};

struct SQLite_TableExistsContext {
    bool texists;
};

class SQLiteDb {
    public:
        SQLiteDb(std::string);
        static int tablesCount(void *, int, char **, char **);
        static int existsTable(void *, int, char **, char **);
        static int printAll(void *, int, char **, char **);
        ~SQLiteDb();

    private:
        sqlite3 * db;
        void checkRC(int, std::string, char *);
};