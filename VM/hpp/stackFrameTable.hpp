#ifndef _STACKFRAMETABLE
#define _STACKFRAMETABLE
#include "./property.hpp"
struct StackFrameItem
{
    u64 baseOffset;
    u64 autoUnwinding;//需要自动释放的变量数量
    u64 isTryBlock;//是否为tryBlock
    u64 isFunctionBlock;//是否为函数block
    u64 size;//帧大小
    u64 length;//变量数量
    PropertyItem* items;
};
class StackFrameTable
{
private:
    char* buffer;

public:
    u64 length;
    StackFrameItem** items;
    StackFrameTable(const char* filename);
    ~StackFrameTable();
};
#endif