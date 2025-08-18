#pragma once
namespace Cache
{

template<typename Key, typename Value>
class Policy
{
public:
    // 虚析构 派生类正确析构
    virtual ~Policy() {};

    // 添加放入页接口
    virtual void put(Key key, Value value) = 0;

    // 获取页接口
    // 直接在传入引用中修改
    virtual bool get(Key key, Value &value) = 0;
    // 返回Value 无则返回nullptr
    virtual Value get(Key key) = 0;


};

}   // namespace Cache