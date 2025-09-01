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
        
    }
};






};