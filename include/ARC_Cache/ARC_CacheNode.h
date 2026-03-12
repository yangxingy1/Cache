#pragma once

#include <memory>

namespace Cache
{

template<typename Key, typename Value>
class ArcNode
{
private:
    Key key;
    Value value;
    size_t accessCount;
    std::weak_ptr<ArcNode> prev;
    std::shared_ptr<ArcNode> next;

public:
    ArcNode() : accessCount(1), next(nullptr) {}

    ArcNode(Key key, Value value)
    : key(key)
    , value(value)
    , accessCount(1)
    , next(nullptr)
    {}

    Key getKey() const { return key; }
    Value getValue() const { return value; }
    size_t getAccessCount() const { return accessCount; }

    void setValue(const Value& value) { this -> value = value; }
    void incrementAccessCount() { ++accessCount; }

    template<typename K, typename V> friend class ARC_lruPart;
    template<typename K, typename V> friend class ARC_lfuPart;

};

} // namespace Cache