#include "SQLite.h"
#include <iostream>

SQL_l::SQL_l(const std::string& dbName) 
{
    if (!openDatabase(dbName)) 
    {
        std::cerr << "打开数据库失败！\n";
    }
}

SQL_l::~SQL_l() 
{
    SQL_l::closeDatabase();
}

bool SQL_l::openDatabase(const std::string& dbName) 
{
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc) 
    {
        std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

void SQL_l::closeDatabase() 
{
    if (db) 
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

std::string SQL_l::Query(const std::string& key,const std::string& key_n, const std::string& value_n, const std::string& table_n)
{
    queryResult.clear();

    std::string sql = "SELECT " + value_n + " FROM " + table_n +
                      " WHERE " + key_n + " = " + key + ";";

    char* errMsg = nullptr;

    int rc = sqlite3_exec(db, sql.c_str(), callback, this, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error in Query: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return "";
    }

    return queryResult;
}

int SQL_l::callback(void* data, int argc, char** argv, char** azColName) 
{
    SQL_l* self = static_cast<SQL_l*>(data); // 转回 this 指针
    if (argc > 0 && argv[0]) 
        self->queryResult = argv[0]; // 仅保存第一列的数据
    return 0;
}

bool SQL_l::executeQuery(const std::string& query) 
{
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), callback, nullptr, &errMsg);
    if (rc != SQLITE_OK) 
    {
        std::cerr << "执行SQL出错: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SQL_l::insertData(const std::string& insertSQL) 
{
    return executeQuery(insertSQL);
}

void SQL_l::printAll(const std::string& tableName) 
{
    std::string query = "SELECT * FROM " + tableName + ";";
    executeQuery(query);
}
