#pragma once

#include "ARC_CacheNode.h"

#include <unordered_map>
#include <mutex>

namespace Cache
{

template<typename Key, typename Value>
class ARC_lruPart
{
public:
    using NodeType = ArcNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;

private:
    size_t capacity;
    size_t ghostCapacity;
    size_t transformThreshold;      // 转换阈值
    std::mutex mutex;

    NodeMap mainCache;
    NodeMap ghostCache;

    NodePtr mainHead;
    NodePtr mainTail;

    NodePtr ghostHead;
    NodePtr ghostTail;

    void initializeLists()
    {
        mainHead = std::make_shared<NodeType>();
        mainTail = std::make_shared<NodeType>();
        mainHead -> next = mainTail;
        mainTail -> prev = mainHead;

        ghostHead = std::make_shared<NodeType>();
        ghostTail = std::make_shared<NodeType>();
        ghostHead -> next = ghostTail;
        ghostTail -> prev = ghostHead;
    }

    bool updateExistingNode(NodePtr node, const Value& value)
    {
        node -> setValue(value);
        moveToFront(node);
        return true;
    }

    bool addNewNode(const Key& key, const Value& value)
    {
        if(mainCache.size() >= capacity)
            evictLeastRecent();
        
        NodePtr newNode = std::make_shared<NodeType>(key, value);
        mainCache[key] = newNode;
        addToFront(newNode);
        return true;
    }

    bool updateNodeAccess(NodePtr node)
    {
        moveToFront(node);
        node -> incrementAccessCount();
        return node -> getAccessCount() >= transformThreshold;
    }

    void moveToFront(NodePtr node)
    {
        if(!node -> prev.expired() && node -> next)
        {
            auto pre = node->prev.lock();
            pre -> next = node -> next;
            node -> next -> prev = node -> prev;
            node -> next = nullptr;
        }

        addToFront(node);
    }

    void addToFront(NodePtr node)
    {
        node -> next = mainHead -> next;
        node -> prev = mainHead;
        mainHead -> next -> prev = node;
        mainHead -> next = node;
    }

    void evictLeastRecent()
    {
        NodePtr leastRecent = mainTail -> prev.lock();
        if(!leastRecent || leastRecent == mainHead)
            return;
        
        removeFromMain(leastRecent);

        if(ghostCache.size() >= ghostCapacity)
            removeOldestGhost();
        
        addToGhost(leastRecent);

        mainCache.erase(leastRecent->getKey());
    }

    void removeFromMain(NodePtr node)
    {
        if(!node->prev.expired() && node->next)
        {
            auto pre = node -> prev.lock();
            pre -> next = node -> next;
            node -> next ->prev = node -> prev;
            node -> next = nullptr;
        }
    }

    void removeFromGhost(NodePtr node) 
    {
        if (!node->prev_.expired() && node->next_) {
            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = node->prev_;
            node->next_ = nullptr; // 清空指针，防止悬垂引用
        }
    }

    void addToGhost(NodePtr node)
    {
        node->accessCount = 1;

        node -> next = ghostHead->next;
        node->prev = ghostHead;
        ghostHead -> next -> prev = node;
        ghostHead -> next = node;

        ghostCache[node->getKey()] = node;
    }

    void removeOldestGhost() 
    {
        NodePtr oldestGhost = ghostTail_->prev_.lock();
        if (!oldestGhost || oldestGhost == ghostHead_) 
            return;

        removeFromGhost(oldestGhost);
        ghostCache_.erase(oldestGhost->getKey());
    }

public:
    explicit ARC_lruPart(size_t capacity, size_t transformThreshold)
    : capacity(capacity)
    , ghostCapacity(capacity)
    , transformThreshold(transformThreshold)
    {
        initializeLists();
    }

    bool put(Key key, Value value)
    {
        if(capacity == 0) return false;

        std::lock_guard<std::mutex> lock(mutex);
        auto it = mainCache.find(key);
        if(it != mainCache.end())
            return updateExistingNode(it->second, value);
        
        return addNewNode(key, value);
    }

    bool get(Key key, Value& value, bool& shouldTransform)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = mainCache.find(key);
        if(it != mainCache.end())
        {
            shouldTransform = updateNodeAccess(it->second);
            value = it -> second -> getValue();
            return true;
        }
        return false;
    }

    bool checkGhost(Key key)
    {
        auto it = ghostCache.find(key);
        if(it != ghostCache.end())
        {
            removeFromGhost(it -> second);
            ghostCache.erase(it);
            return true;
        }
        return false;
    }

    void increaseCapacity() { ++capacity; }

    bool decreaseCapacity()
    {
        if(capacity <= 0) return false;
        if(mainCache.size() == capacity)
            evictLeastRecent();
        
        --capacity;
        return true;
    }
};

} // namespace Cache