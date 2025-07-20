#include "customUtils.h"
#include "SQLite_db.h"

#include <filesystem>
#include <string>

SQLiteDb::SQLiteDb(std::string name) {
    // checking for db directory existance
    if (!std::filesystem::is_directory("db")) {
        print("Cannot find db folder, creating new one\n");
        std::filesystem::create_directory("db");
    }

    int rc;
    char * db_path = ("db/" + name + ".db").data();

    // checking for db file existance
    if (!std::filesystem::exists(db_path)) {
        print(name + ".db does not exist, will create a new one\n");
    }

    rc = sqlite3_open(db_path, &db);
    checkRC(rc, "Failed to open database.", nullptr);

    print("Opened " + name + ".db\n");

    // checking for table count
    char * errMsg;

    SQLite_TableCountContext countContext;
    rc = sqlite3_exec(
        db,
        "SELECT count(*) FROM sqlite_master WHERE type='table';",
        tablesCount,   // function callback
        &countContext,
        &errMsg
    );
    checkRC(rc, "Failed to check table count.", errMsg);

    print(countContext.tcount);
    print(" existing tables found\n");

    if (countContext.tcount == 0) {
        // to do ::
        // rc = sqlite3_exec(
        //     db,
        //     "CREATE TABLE metadata ()",
        //     NULL,
        //     NULL,
        //     &errMsg
        // );
        // checkRC(rc, "Failed to create table.", errMsg);
    }

    SQLite_TableExistsContext existsContext;
    rc = sqlite3_exec(
        db,
        "SELECT * FROM sqlite_master WHERE name='metadata';",
        existsTable,
        &existsContext,
        &errMsg
    );
    checkRC(rc, "Failed to checking existing table.", errMsg);
}

int SQLiteDb::tablesCount(void * context, int argc, char **argv, char **azColName) {
    // this returns table count by using a struct pointer as context
    SQLite_TableCountContext * tableData = (SQLite_TableCountContext *) context;
    if (tableData && tableData->tcount) {
        tableData->tcount = *argv[0] - '0';
    }
    return 0;
}

int SQLiteDb::existsTable(void * unused, int argc, char **argv, char **azColName) {
    print("existsTable:\n");
    for (int i = 0; i < argc; i++) {
        print(azColName[i]);
        print(" ");
        print(argv[i] ? argv[i] : "NULL");
        print("\n");
    }
    print(":existsTable\n");
    return 0;
}






int SQLiteDb::printAll(void * unused, int argc, char **argv, char **azColName) {
    print("printAll:\n");
    for (int i = 0; i < argc; i++) {
        print(azColName[i]);
        print(" ");
        print(argv[i] ? argv[i] : "NULL");
        print("\n");
    }
    print(":printAll\n");
    return 0;
}

SQLiteDb::~SQLiteDb() {
    sqlite3_close(db);
}

void SQLiteDb::checkRC(int rc, std::string message, char * errorMsg) {
    if (rc != SQLITE_OK) {
        std::string temp = std::string(errorMsg);
        exception(message + " Err: " + temp);
    }
}