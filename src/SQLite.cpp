#include "SQLite.h"
#include <iostream>

SQL::SQL(const std::string& dbName) {
    if (!openDatabase(dbName)) {
        std::cerr << "无法打开数据库\n";
    }
}

SQL::~SQL() {
    closeDatabase();
}

bool SQL::openDatabase(const std::string& dbName) {
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc) {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

void SQL::closeDatabase() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

int SQL::callback(void* NotUsed, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; ++i) {
        std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    std::cout << std::endl;
    return 0;
}

bool SQL::executeQuery(const std::string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), callback, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "执行SQL出错: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SQL::insertData(const std::string& insertSQL) {
    return executeQuery(insertSQL);
}

void SQL::printAll(const std::string& tableName) {
    std::string query = "SELECT * FROM " + tableName + ";";
    executeQuery(query);
}
