#include<string>
#include<memory>
#include<unordered_map>
#include<list>
#include<mutex>
#include<vector>
#include "CachePolicy.h"

namespace Cache
{
// 前向声明模板类       ->  在node类中声明友元时提前给出模板类声明
template<typename Key, typename Value>
class LRUCache;

// 缓存结点结构 -> 双向链表 + 哈希表
template<typename Key, typename Value>
class LRUNode
{
private:
    // 键值 访问次数 前/后向指针
    Key key;
    Value value;
    size_t access_times;
    std::weak_ptr<LRUNode<Key, Value>> prev;
    std::shared_ptr<LRUNode<Key, Value>> next;

public:
    LRUNode(Key key, Value value) : key(key), value(value), access_times(1) {}
    // 获取key value access_times 设置value 访问结点
    Key getKey() const {return key;}
    Value getValue() const {return value;}
    void setValue(const Value &value) {this->value = value;}
    size_t getAccessTimes() const {return access_times;}
    void increaseAccessTimes() {++access_times;}

    friend class LRUCache<Key, Value>;
};


template<typename Key, typename Value>
class LRUCache : public Policy<Key, Value>
{
public:
    using NodeType = LRUNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
private:
    // 容量  
    int capacity;
    // 哈希表 锁 头尾指针
    NodeMap nodeMap;
    std::mutex mutex_;
    NodePtr head;
    NodePtr tail;
    
    // 初始化双向链表和哈希表
    void initializeList()
    {
        // 创建头尾结点
        head = std::make_shared<NodeType>(Key{}, Value{});
        tail = std::make_shared<NodeType>(Key{}, Value{});

        head->next = tail;
        tail->prev = head;
    }

    // 插入新节点(尾结点)
    void insertNode(NodePtr node)
    {
        node->next = tail;
        node->prev = tail->prev;
        tail->prev.lock()->next = node;
        tail->prev = node;
    }

    // 删去结点
    void removeNode(NodePtr node)
    {
        if(!node->prev.expired() && node->next)
        {
            node->prev.lock()->next = node->next;
            node->next->prev = node->prev;
            node->next = nullptr;
        }
    }

    // 驱逐最近最久未使用结点
    void removeLeastRecent()
    {
        NodePtr least = head->next;
        removeNode(least);
        nodeMap.erase(least->getKey());
    }

    // 将结点移动到最近访问位置
    void moveToMostRecent(NodePtr node)
    {
        removeNode(node);
        insertNode(node);
    }

    // 添加新节点(若Cache满则先驱逐最近最久未使用)
    void addNewNode(const Key& key, const Value& value)
    {   
        if(nodeMap.size() >= capacity)
            removeLeastRecent();

        NodePtr newNode = std::make_shared<NodeType>(key, value);
        insertNode(newNode);
        nodeMap[key] = newNode;
    }

    // 更新某结点的value值
    void updateExistingNode(NodePtr node, const Value& value)
    {
        node->setValue(value);
        moveToMostRecent(node);
    }

public:
    // 构造函数
    LRUCache(int capacity)
    {
        this->capacity = capacity;
        initializeList();
    }

    // 默认析构
    ~LRUCache() override = default;

    // 放入缓存   
    void put(Key key, Value value) override
    {
        if(capacity <= 0)
            return ;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            // 缓存在Cache容器中已经存在    这里对内容进行更新覆写  增加访问次数记录
            it->second->increaseAccessTimes();

            updateExistingNode(it->second, value);
            return;
        }
        addNewNode(key, value);
    }

    // 从缓存中获取值(直接在传入引用中返回value)
    bool get(Key key, Value& value) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            // 访问该节点
            moveToMostRecent(it->second);
            // 增加访问次数
            it->second->increaseAccessTimes();
            value = it->second->getValue();
            return true;
        }
        return false;
    }

    // 从缓存中获取值(作为返回值返回value)
    Value get(Key key) override
    {
        Value value{};
        get(key, value);
        return value;
    }

    // 删除指定页
    void remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            removeNode(it->second);
            nodeMap.erase(key);
        }
    }

    // 获取结点访问次数接口     便于比较不同算法及优化
    size_t getAccessTime(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
            return it->second->getAccessTime();

        return 0;
    }
};

// LRU-K: 在LRU基础上增加判断条件，访问次数达到K次后才加入缓存
template<typename Key, typename Value>
class LRU_KCache : public LRUCache<Key, Value>
{
private:
    int k;                                                  // 进入主缓存的访问次数阈值
    std::unique_ptr<LRUCache<Key, size_t>> historyList;     // 每个页的访问次数 : 历史队列
    std::unordered_map<Key, Value> historyValueMap;         // 存储未达到K次的数据
    std::mutex mutex_;                                      // 保护子类的get/put

public:
    LRU_KCache(int capacity, int historyCapacity, int k) 
        : LRUCache<Key, Value>(capacity)
        , historyList(std::make_unique<LRUCache<Key, size_t>>(historyCapacity))
        , k(k)
    {}

    Value get(Key key)
    {
        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(mutex_);
        
        Value value{};

        // 更新历史记录队列中的访问次数
        size_t getTimes = historyList->get(key);
        getTimes++;
        historyList->put(key, getTimes);

        // 查看是否在主缓存中
        bool inMainCache = LRUCache<Key, Value>::get(key, value);
        if(inMainCache)
            return value;
        
        if(getTimes >= k)
        {
            auto it = historyValueMap.find(key);
            if(it != historyValueMap.end())
            {
                // 保存历史值 放入主缓存中
                Value historyValue = it->second;
                LRUCache<Key, Value>::put(key, historyValue);

                // 从历史队列、哈希表中删去
                historyList->remove(key);
                historyValueMap.erase(it);

                return historyValue;
            }

        }
        // 不在主缓存中且访问次数 < k
        return value;
    }

    void put(Key key, Value value)
    {
        Value existingValue{};
        bool inMainCache = LRUCache<Key, Value>::get(key, existingValue);

        // 查看是否在主缓存中已经存在
        if(inMainCache)
        {
            // 存在 -> 直接放入
            LRUCache<Key, Value>::put(key, value);
            return;
        }

        // 更新历史记录队列中的访问次数
        size_t getTimes = historyList->get(key);
        getTimes++;
        historyList->put(key, getTimes);

        // 保存键值对
        historyValueMap[key] = value;

        // 检查是否达到访问阈值 -> 放入主缓存
        if(getTimes > k)
        {
            historyList->remove(key);
            historyValueMap.erase(key);
            LRUCache<Key, Value>::put(key, value);
        }
    }
};

// LRU-Slice: 分片LRU缓存 把缓存分片供多个线程取用 增强并发性能
template<typename Key, typename Value>
class LRU_HashCache : public LRUCache<Key, Value>
{
private:
    size_t capacity;                                                        //总容量
    int sliceNum;                                                           //分片数
    std::vector<std::unique_ptr<LRUCache<Key, Value>>> LRU_SliceCaches;      //切片缓存

    // 把key转换成对应的哈希值
    static size_t Hash(Key key) const
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }

public:
    // 构造函数 -> 如果分片数未指定/不合法则使用CPU核心数
    LRU_HashCache(size_t capacity, int sliceNum)
        : capacity(capacity)
        , sliceNum(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
    {
        size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum)); // 向上取整计算每个分片容量
        for(int i=0; i<sliceNum; i++)
            LRU_SliceCaches.emplace_back(new LRUCache<Key, Value>(sliceNum));
    }

    void put(Key key, Value value)
    {
        // 计算出对应的分片位置并放入值
        size_t position = Hash(key) % sliceNum;
        LRU_SliceCaches[position]->put(key, value);
    }

    bool get(Key key, Value& value)
    {
        // 计算出分片位置并获取值
        size_t position = Hash(key) % sliceNum;
        return LRU_SliceCaches[position]->get(key, value);
    }

    Value get(Key key)
    {
        // 调用get(key, & value)方法
        Value value{};
        get(key, value);
        return value;
    }
};

}   // namespace Cache