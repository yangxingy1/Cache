#include<cmath>
#include<mutex>
#include<memory>
#include<thread>
#include<unordered_map>
#include<vector>

#include "CachePolicy.h"

namespace Cache
{
    
template<typename Key, typename Value> class LFU_Cache;

// 新加入的结点使用尾插法插入尾部   
// 访问次数增加后需删去的结点直接拿出
// 在页置换时需要舍弃的结点从头部开始拿出(最低频中最老的结点先淘汰)
template<typename Key, typename Value>
class NodeList
{
private:
    struct Node
    {
        // 访问频次 键值 前后节点指针
        int freq;
        Key key;                
        Value value;
        std::shared_ptr<Node> next;
        std::weak_ptr<Node> pre;

        Node()
        : freq(1), next(nullptr) {}
        Node(Key key, Value value)
        : freq(1), key(key), value(value), next(nullptr) {}

    };
    using NodePtr = std::shared_ptr<Node>;
    int freq;           // 列表中所有节点访问的频次
    NodePtr head;       // 头结点
    NodePtr tail;       // 尾结点

public:
    explicit NodeList(int n)
    : freq(n)
    {
        head = std::make_shared<Node>();
        tail = std::make_shared<Node>();
        head->next = tail;
        tail->pre = head;
    }

    bool isEmpty()
    {
        return head->next == tail;
    }

    void addNode(NodePtr node)
    {
        if(!head || !tail || !node)
            return;
        
        // 从尾部插入结点
        node->next = tail;
        node->pre = tail->pre;
        tail->pre.lock()->next = node;
        tail->pre = node;
    }

    void removeNode(NodePtr node)
    {
        if(!head || !tail || !node)
            return;
        if(!node->next || node->pre.expired())
            return;
        node->pre.lock()->next = node->next;
        node->next->pre = node->pre;
        // 显示置空next 彻底断开链接
        node->next = nullptr;
    }

    // 获取头部第一个结点用于置换删除
    NodePtr getFirstNode() const {  return head->next;  }

};

template<typename Key, typename Value>
class LFU_Cache : Policy<Key, Value>
{
    using Node = typename NodeList<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
private:
    int capacity;           // 最大容量
    int minFreq;            // 最低访问频次
    int maxAverageNum;      // 最大平均访问频次
    int curTotalNum;        // 当前总访问频次
    int curAverageNum;      // 当前平均访问频次
    std::mutex mutex;       // 互斥锁
    NodeMap nodeMap;        // key -> 缓存结点
    std::unordered_map<int, NodeList<Key, Value>*> freqToFreqList;      // 访问频次 -> 对应列表 

private:
    // 把新结点放入对应的访问频次列表中
    void addToFreqList(NodePtr node)
    {
        if(!node)
            return;

        auto freq = node->freq;
        // 没有则新增列表
        if(freqToFreqList.find(freq) == freqToFreqList.end())
            freqToFreqList[freq] = new NodeList<Key, Value>(freq);
        
        freqToFreqList[freq]->addNode(node);
    }

    // 把结点从对应的访问频次列表中删去
    void removeFromFreqList(NodePtr node)
    {
        if(!node)
            return;

        auto freq = node->freq;
        // 没有则直接返回
        if(freqToFreqList.find(freq) == freqToFreqList.end())
            return;
        freqToFreqList[freq]->removeNode(node);
    }

    // 访问次数+1
    void addFreqNum()
    {
        curTotalNum++;
        if(nodeMap.empty())
            curAverageNum = 0;
        else
            curAverageNum = curTotalNum / nodeMap.size();
        
        if(curTotalNum > maxAverageNum)
            handleOverMaxAverageNum();
        
    }

    // 更新最小频率
    void updateMinFreq()
    {
        minFreq = INT8_MAX;
        // 遍历找出最小的访问频率
        for(const auto& pair : nodeMap)
            if(!pair.second && !pair.second->isEmpty())
                minFreq = std::min(minFreq, pair.first);
        if(minFreq == INT8_MAX)
            minFreq = 1;
    }

    // 减少平均访问次数和总频次 -> 便于在长时间累计时仍然保持缓存内容的持续更新
    void decreaseFreqNum(int num)
    {
        curTotalNum -= num;
        if(nodeMap.empty())
            curAverageNum = 0;
        else
            curAverageNum = curTotalNum / nodeMap.size();
    }

    // 处理超过最大平均访问次数时的情况 -> -=MaxAverageNum / 2  后重新计数
    void handleOverMaxAverageNum()
    {
        if(nodeMap.empty())
            return;

        // 所有的节点频率 -= MaxAverageNum / 2
        for(auto it : nodeMap)
        {
            if(!it->second)
                continue;
            NodePtr node = it->second;

            removeFromFreqList(node);
            node->freq = (node->freq - maxAverageNum / 2) > 1 ? node->freq - maxAverageNum / 2: 1;
            addToFreqList(node);
        }
        updateMinFreq();
    }

    // key 不在缓存中时放入
    void putInternel(Key key, Value value)
    {
        // 判断缓存容量
        if(nodeMap.size() == capacity)
            kickOut();

        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap[key] = node;
        addToFreqList(node);
        addFreqNum();
        minFreq = std::min(minFreq, 1);
    }

    // 缓存满时移除最早最少访问结点
    void kickOut()
    {
        NodePtr node = freqToFreqList[minFreq]->getFirstNode();
        removeFromFreqList(node);
        nodeMap.erase(node->key);
        decreaseFreqNum(node->freq);
    }

    // 从缓存中获取value
    void getInternel(NodePtr node, Value& value)
    {
        value = node->value;

        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);

        // 此时freq = minFreq + 1且之前的列表为空   则最小访问频次+1
        if(node->freq == minFreq + 1 && freqToFreqList[minFreq]->isEmpty())
            minFreq++;

        // 更新访问频次
        addFreqNum();
    }
    
public:
    LFU_Cache(int capacity, int maxAverageNum=1000000)
    : capacity(capacity), minFreq(INT8_MAX), maxAverageNum(maxAverageNum)
    , curAverageNum(0), curTotalNum(0)
    {}

    ~LFU_Cache() override = default;

    void put(Key key, Value value) override
    {
        if(capacity == 0)
            return;
        std::lock_guard<std::mutex> lock(mutex);
        if(nodeMap.find(key) != nodeMap.end())
        {
            nodeMap[key]->value = value;
            // 访问次数+1
            getInternel(nodeMap[key], value);
            return;
        }
        putInternel(key, value);
    }

    bool get(Key key, Value& value) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            getInternel(it->second, value);
            return true;
        }
        return false;
    }

    Value get(Key key) override
    {
        Value value;
        get(key, value);
        return value;
    }

    // 清空缓存 回收资源
    void purge()
    {
        nodeMap.clear();
        freqToFreqList.clear();
    }
};



};