#include<string>
#include<memory>
#include<unordered_map>
#include<list>
#include<mutex>

#include "CachePolicy.h"

namespace Cache
{

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
};


template<typename Key, typename Value>
class LRUCache : public Policy<Key, Value>
{
public:
    using NodeType = LRUNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodeType>;
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

public:


};




}