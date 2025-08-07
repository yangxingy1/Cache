#include "include/LRU_CachePolicy.h"
#include "include/SQLite.h"
#include<iostream>
#include<chrono>
#include<random>
#include<vector>
#include<iomanip>


using namespace Cache;
using std::string, std::to_string, std::cout;

void printResult(string testName, const int capacity, unsigned int& getTimes, unsigned int& hitTimes)
{
    cout << "=== " << testName << " 结果汇总 ===\n";
    cout << "缓存大小: " << capacity << "\n";
    double hitRate = (double)hitTimes / getTimes * 100;
    cout << testName << ": " << "命中率: " << std::fixed << std::setprecision(2) << hitRate << "%";
    cout << "(" << hitTimes << "/" << getTimes << ")" << std::endl;
}

void testHotDataAccess()
{
    // 定义容量 访问次数    热数据量    冷数据量
    const int capacity = 20;
    const int operatorTimes = 500000;
    const int hotKeys = 20;
    const int coldkeyS = 5000;

    // 随机生成key
    std::random_device rd;
    std::mt19937 gen(rd());

    // 读次数与命中次数 -> 求命中率
    unsigned int getTimes = 0;
    unsigned int hitTimes = 0;

    LRUCache<int, string> cache(capacity);
    //  插入热数据预热缓存
    for(int key=0; key < hotKeys;key++)
    {
        string value = to_string(key);
        cache.put(key, value);
    }

    // 交替put get
    for(int op=0; op < operatorTimes; op++)
    {
        // 30%概率写    70%概率读
        bool isPut = (gen() % 100 < 30);
        int key;

        // 70%概率热数据    30%冷数据
        bool isHot = (gen() % 100 < 70);
        if(isHot)
            key = gen() % hotKeys;
        else
            key = gen() % coldkeyS + hotKeys;
        
        if(isPut)
        {
            string value = to_string(key) + "_" + to_string(op % 100);
            cache.put(key, value);
        }
        else
        {
            getTimes++;
            string value;
            if(cache.get(key, value))
                hitTimes++;
        }
    }
    printResult("LRU", capacity, getTimes, hitTimes);
}


int main()
{
    std::cout << "hello world!" << std::endl;
    SQL sql("source.db");
    sql.printAll("pages");
    // testHotDataAccess();
    return 0;
}