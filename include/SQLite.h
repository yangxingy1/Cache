#ifndef SQLITEWRAPPER_H
#define SQLITEWRAPPER_H

#include <string>
#include "../lib/SQLite3/sqlite3.h"


class SQL_l {
public:
    SQL_l(const std::string& dbName);
    ~SQL_l();

    bool executeQuery(const std::string& query);
    bool insertData(const std::string& insertSQL);
    void printAll(const std::string& tableName);
    std::string Query(const std::string& key,
                      const std::string& key_n,
                      const std::string& value_n,
                      const std::string& table_n);

private:
    sqlite3* db;
    std::string queryResult;
    bool openDatabase(const std::string& dbName);
    void closeDatabase();
    static int callback(void* NotUsed, int argc, char** argv, char** azColName);
};
#endif
