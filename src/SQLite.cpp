#include "SQLite.h"
#include <iostream>

SQL_l::SQL_l(const std::string& dbName) {
    if (!openDatabase(dbName)) {
        std::cerr << "打开数据库失败！\n";
    }
}

SQL_l::~SQL_l() {
    SQL_l::closeDatabase();
}

bool SQL_l::openDatabase(const std::string& dbName) {
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc) {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

void SQL_l::closeDatabase() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

int SQL_l::callback(void* NotUsed, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; ++i) {
        std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    std::cout << std::endl;
    return 0;
}

bool SQL_l::executeQuery(const std::string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), callback, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "执行SQL出错: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SQL_l::insertData(const std::string& insertSQL) {
    return executeQuery(insertSQL);
}

void SQL_l::printAll(const std::string& tableName) {
    std::string query = "SELECT * FROM " + tableName + ";";
    executeQuery(query);
}
