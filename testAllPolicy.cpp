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

void printResult(const int capacity, 
                    const std::vector<string>& policyName, 
                    const std::vector<unsigned int>& getTimes, 
                    const std::vector<unsigned int>& hitTimes)
{
    for(int i=0; i<policyName.size(); i++)
    {
        cout << "============== " << policyName[i] << ": ==============\n";
        cout << "缓存大小: " << capacity << "\n";
        double hitRate = (double)hitTimes[i] / getTimes[i] * 100;
        cout << policyName[i] << ": " << "命中率: " << std::fixed << std::setprecision(2) << hitRate << "%";
        cout << "(" << hitTimes[i] << "/" << getTimes[i] << ")" << std::endl;
        cout << "===================================\n" << std::endl;
    }

}

void testHotDataAccess(SQL_l& source)
{
    cout << "\n=== 测试场景1: 热点数据访问测试 ===\n" << std::endl;

    // 定义容量 访问次数    热数据量    冷数据量    策略名称
    const int capacity = 20;
    const int operatorTimes = 500000;
    const int hotKeys = 20;
    const int coldKeys = 5000;
    std::vector<string> cacheNames = {"LRU", "LRU-K"};
    // 随机生成key
    std::random_device rd;
    std::mt19937 gen(rd());

    // 读次数与命中次数 -> 求命中率
    // 结果存储
    std::vector<unsigned int> getTimes(2, 0);
    std::vector<unsigned int> hitTimes(2, 0);

    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, hotKeys+coldKeys, 2);
    
    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache};
    

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
                getTimes[i]++;
                string value;
                // 命中则不变
                if(policy->get(key, value))
                    hitTimes[i]++;
                // 未命中则放入
                else
                {
                    value = source.Query(to_string(key), "key", "value", "Pages");
                    policy->put(key, value);
                }
            }
        }
        i++;
    }
    printResult(capacity, cacheNames, getTimes, hitTimes);
}

void testLoopPattern(SQL_l& source) 
{
    cout << "\n=== 测试场景2：循环扫描测试 ===\n" << std::endl;
    // 定义容量     循环范围    操作次数    策略名称
    const int capacity = 50;          
    const int loopSize = 500;        
    const int operations = 200000;    
    std::vector<string> cacheNames = {"LRU", "LRU-K"};

    // 随机生成key
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, loopSize * 2, 2);

    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache};
    std::vector<unsigned int> hitTimes(2, 0);
    std::vector<unsigned int> getTimes(2, 0);


    // 为每种缓存算法运行相同的测试
    for (int i = 0; i < caches.size(); ++i) {
        // 先预热一部分数据（只加载20%的数据）
        for (int key = 0; key < loopSize / 5; ++key) {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 设置循环扫描的当前位置
        int current_pos = 0;
        
        // 交替进行读写操作，模拟真实场景
        for (int op = 0; op < operations; ++op) {
            // 20%概率是写操作，80%概率是读操作
            bool isPut = (gen() % 100 < 20);
            int key;
            
            // 按照不同模式选择键
            if (op % 100 < 60) {  // 60%顺序扫描
                key = current_pos;
                current_pos = (current_pos + 1) % loopSize;
            } else if (op % 100 < 90) {  // 30%随机跳跃
                key = gen() % loopSize;
            } else {  // 10%访问范围外数据
                key = loopSize + (gen() % loopSize);
            }
            
            if (isPut) {
                // 执行put操作，更新数据
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 执行get操作并记录命中情况
                std::string result;
                getTimes[i]++;
                if (caches[i]->get(key, result)) {
                    hitTimes[i]++;
                }
            }
        }
    }

    printResult(capacity, cacheNames, getTimes, hitTimes);
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