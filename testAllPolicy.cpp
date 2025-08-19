#include "include/CachePolicy.h"
#include "include/LRU_CachePolicy.h"
#include "data/SQLite.h"
#include<iostream>
#include<chrono>
#include<ctime>
#include<random>
#include<vector>
#include<iomanip>


using namespace Cache;
using std::string, std::to_string, std::cout;

void printResult(string testName, const int capacity, unsigned int& getTimes, unsigned int& hitTimes)
{
    cout << "========= " << testName << ": ============\n";
    cout << "缓存大小: " << capacity << "\n";
    double hitRate = (double)hitTimes / getTimes * 100;
    cout << testName << ": " << "命中率: " << std::fixed << std::setprecision(2) << hitRate << "%";
    cout << "(" << hitTimes << "/" << getTimes << ")" << std::endl;
    cout << "=================================\n" << std::endl;
}

void testHotDataAccess(SQL_l& source)
{
    cout << "\n=== 测试场景1: 热点数据访问测试 ===\n" << std::endl;

    // 定义容量 访问次数    热数据量    冷数据量
    const int capacity = 20;
    const int operatorTimes = 500000;
    const int hotKeys = 20;
    const int coldKeys = 5000;

    // 随机生成key
    std::random_device rd;
    std::mt19937 gen(rd());

    // 读次数与命中次数 -> 求命中率
    unsigned int getTimes = 0;
    unsigned int hitTimes = 0;

    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, hotKeys+coldKeys, 2);

    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache};
    std::vector<string> cacheNames = {"LRU", "LRU-K"};

    // 策略名称计数器
    int i = 0;
    for(auto policy:caches)
    {
        //  插入热数据预热缓存
        for(int key=0; key < hotKeys;key++)
        {
            string value = to_string(key);
            policy->put(key, value);
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
                key = gen() % coldKeys + hotKeys;
            
            // 如果为写操作
            if(isPut)
            {
                string value = to_string(key) + "_" + to_string(op % 100);
                policy->put(key, value);
            }
            // 如果为读操作
            else
            {
                getTimes++;
                string value;
                // 命中则不变
                if(policy->get(key, value))
                    hitTimes++;
                // 未命中则放入
                else
                {
                    value = source.Query(to_string(key), "key", "value", "Pages");
                    policy->put(key, value);
                }
            }
        }
        printResult(cacheNames[i], capacity, getTimes, hitTimes);
        i++;
        getTimes = 0;
        hitTimes = 0;
    }
}


int main()
{
    cout << "hello world!" << std::endl;
    SQL_l sql("source.db");
    // sql.executeQuery("CREATE TABLE IF NOT EXISTS Pages (id INTEGER PRIMARY KEY AUTOINCREMENT, key INTEGER unique, value TEXT);");
    testHotDataAccess(sql);
    int a;
    std::cin >> a;
    return 0;
}