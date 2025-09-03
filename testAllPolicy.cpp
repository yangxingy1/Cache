#include "include/CachePolicy.h"
#include "include/LRU_CachePolicy.h"
#include "include/LFU_CachePolicy.h"
#include "data/SQLite.h"
#include<iostream>
#include<chrono>
#include<ctime>
#include<random>
#include<vector>
#include<iomanip>


using namespace Cache;
using std::string, std::to_string, std::cout;

static std::vector<string> cacheNames = {"LRU", "LRU-K", "LRU-Hash", "LFU", "LFU-Hash"};


void printResult(const int capacity, 
                    const std::vector<unsigned int>& getTimes, 
                    const std::vector<unsigned int>& hitTimes)
{
    for(int i=0; i<cacheNames.size(); i++)
    {
        cout << "============== " << cacheNames[i] << ": ==============\n";
        cout << "缓存大小: " << capacity << "\n";
        double hitRate = (double)hitTimes[i] / getTimes[i] * 100;
        cout << cacheNames[i] << ": " << "命中率: " << std::fixed << std::setprecision(2) << hitRate << "%";
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

    // 读次数与命中次数 -> 求命中率
    // 结果存储
    std::vector<unsigned int> getTimes(5, 0);
    std::vector<unsigned int> hitTimes(5, 0);

    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, hotKeys+coldKeys, 2);
    LRU_HashCache<int, string> LRU_Hash_cache(capacity, 4);

    LFUCache<int, string> LFUcache(capacity);
    LFU_HashCache<int, string> LFU_Hash_cache(capacity, 4);
    
    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache, &LRU_Hash_cache, &LFUcache, &LFU_Hash_cache};
    

    // 策略名称计数器
    for(int i=0; i<caches.size(); i++)
    {
        // 随机生成key
        std::random_device rd;
        std::mt19937 gen(rd());

        //  插入热数据预热缓存
        for(int key=0; key < hotKeys;key++)
        {
            string value = to_string(key);
            caches[i]->put(key, value);
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
                // cout << "put:" << key << std::endl;
                string value = to_string(key) + "_" + to_string(op % 100);
                caches[i]->put(key, value);
            }
            // 如果为读操作
            else
            {
                getTimes[i]++;
                string value;
                // 命中则不变
                if(caches[i]->get(key, value))
                    hitTimes[i]++;
                // 未命中则放入
                else
                {
                    value = source.Query(to_string(key), "key", "value", "Pages");
                    caches[i]->put(key, value);
                }
            }
        }
    }
    printResult(capacity, getTimes, hitTimes);
}

void testLoopPattern(SQL_l& source) 
{
    cout << "\n=== 测试场景2: 循环扫描测试 ===\n" << std::endl;
    // 定义容量     循环范围    操作次数    策略名称
    const int capacity = 50;          
    const int loopSize = 500;        
    const int operations = 200000;    
    
    std::vector<unsigned int> hitTimes(5, 0);
    std::vector<unsigned int> getTimes(5, 0);
    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, loopSize * 2, 2);
    LRU_HashCache<int, string> LRU_Hash_cache(capacity, 4);

    LFUCache<int, string> LFUcache(capacity);
    LFU_HashCache<int, string> LFU_Hash_cache(capacity, 4);

    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache, &LRU_Hash_cache, &LFUcache, &LFU_Hash_cache};



    // 为每种缓存算法运行相同的测试
    for (int i = 0; i < caches.size(); ++i) 
    {
        // 随机生成key
        std::random_device rd;
        std::mt19937 gen(rd());
        // 先预热一部分数据（只加载20%的数据）
        for (int key = 0; key < loopSize / 5; ++key) 
        {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }
        // 设置循环扫描的当前位置
        int current_pos = 0;

        // 交替进行读写操作，模拟真实场景
        for (int op = 0; op < operations; ++op) 
        {
            // 20%概率是写操作，80%概率是读操作
            bool isPut = (gen() % 100 < 20);
            int key;
            // 按照不同模式选择键
            // 60%顺序扫描
            if(op % 100 < 60) 
            {
                key = current_pos;
                current_pos = (current_pos + 1) % loopSize;
            } 
            // 30%随机跳跃
            else if(op % 100 < 90)          
                key = gen() % loopSize;
            // 10%访问范围外数据
            else  
                key = loopSize + (gen() % loopSize);
            if(isPut) 
            {
                // 执行put操作，更新数据
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } 
            else 
            {
                std::string value;
                getTimes[i]++;
                // 执行get操作并记录命中情况
                if(caches[i]->get(key, value)) 
                    hitTimes[i]++;
                else
                {
                    value = source.Query(to_string(key), "key", "value", "Pages");
                    caches[i]->put(key, value);
                }
            }
        }
    }
    printResult(capacity, getTimes, hitTimes);
}

void testWorkloadShift(SQL_l& source) 
{
    cout << "\n=== 测试场景3: 工作负载剧烈变化测试 ===\n";
    
    const int capacity = 30;            // 缓存容量
    const int operations = 80000;       // 总操作次数
    const int phaseLenth = operations / 5;  // 每个阶段的长度
    
    // 读次数与命中次数 -> 求命中率
    // 结果存储
    std::vector<unsigned int> getTimes(5, 0);
    std::vector<unsigned int> hitTimes(5, 0);

    // 初始化待测缓存
    LRUCache<int, string> LRU_cache(capacity);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    LRU_KCache<int, string> LRU_K_cache(capacity, 500, 2);
    LRU_HashCache<int, string> LRU_Hash_cache(capacity, 4);

    LFUCache<int, string> LFUcache(capacity);
    LFU_HashCache<int, string> LFU_Hash_cache(capacity, 4);
    
    std::vector<Cache::Policy<int, string>*> caches = {&LRU_cache, &LRU_K_cache, &LRU_Hash_cache, &LFUcache, &LFU_Hash_cache};

    std::random_device rd;
    std::mt19937 gen(rd());

    // 为每种缓存算法运行相同的测试
    for (int i = 0; i < caches.size(); ++i) { 
        // 先预热缓存，只插入少量初始数据
        for (int key = 0; key < 30; ++key) {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 进行多阶段测试，每个阶段有不同的访问模式
        for (int op = 0; op < operations; ++op) {
            // 确定当前阶段
            int phase = op / phaseLenth;
            
            // 每个阶段的读写比例不同 
            int putProbability;
            switch (phase) {
                case 0: putProbability = 15; break;  // 阶段1: 热点访问，15%写入更合理
                case 1: putProbability = 30; break;  // 阶段2: 大范围随机，写比例为30%
                case 2: putProbability = 10; break;  // 阶段3: 顺序扫描，10%写入保持不变
                case 3: putProbability = 25; break;  // 阶段4: 局部性随机，微调为25%
                case 4: putProbability = 20; break;  // 阶段5: 混合访问，调整为20%
                default: putProbability = 20;
            }
            
            // 确定是读还是写操作
            bool isPut = (gen() % 100 < putProbability);
            
            // 根据不同阶段选择不同的访问模式生成key - 优化后的访问范围
            int key;
            if (op < phaseLenth) {  // 阶段1: 热点访问 - 热点数量5，使热点更集中
                key = gen() % 5;
            } else if (op < phaseLenth * 2) {  // 阶段2: 大范围随机 - 范围400，更适合30大小的缓存
                key = gen() % 400;
            } else if (op < phaseLenth * 3) {  // 阶段3: 顺序扫描 - 保持100个键
                key = (op - phaseLenth * 2) % 100;
            } else if (op < phaseLenth * 4) {  // 阶段4: 局部性随机 - 优化局部性区域大小
                // 产生5个局部区域，每个区域大小为15个键，与缓存大小20接近但略小
                int locality = (op / 800) % 5;  // 调整为5个局部区域
                key = locality * 15 + (gen() % 15);  // 每区域15个键
            } else {  // 阶段5: 混合访问 - 增加热点访问比例
                int r = gen() % 100;
                if (r < 40) {  // 40%概率访问热点（从30%增加）
                    key = gen() % 5;  // 5个热点键
                } else if (r < 70) {  // 30%概率访问中等范围
                    key = 5 + (gen() % 45);  // 缩小中等范围为50个键
                } else {  // 30%概率访问大范围（从40%减少）
                    key = 50 + (gen() % 350);  // 大范围也相应缩小
                }
            }
            
            if (isPut) 
            {
                // 执行写操作
                string value = "value" + to_string(key) + "_p" + to_string(phase);
                caches[i]->put(key, value);
            } 
            else 
            {
                // 执行读操作并记录命中情况
                string value;
                getTimes[i]++;
                if (caches[i]->get(key, value))
                    hitTimes[i]++;
                else
                {
                    value = source.Query(to_string(key), "key", "value", "Pages");
                    caches[i]->put(key, value);
                }
            }
        }
    }

    printResult(capacity, getTimes, hitTimes);
}

int main()
{
    cout << "hello world!" << std::endl;
    cout << std::thread::hardware_concurrency() << std::endl;
    // 初始化数据库
    SQL_l sql("source.db");
    testHotDataAccess(sql);
    testLoopPattern(sql);
    testWorkloadShift(sql);
    // int a;
    // std::cin >> a;
    return 0;
}