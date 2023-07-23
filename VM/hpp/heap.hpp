#ifndef _HEAP
#define _HEAP
#include "./typeTable.hpp"
enum NativeResourceType
{
    mutex
};
struct HeapItem
{
    union SizeOrLength {
        u64 size;//对于plainObj是size
        u64 length;//对于数组是length
    } sol;//对于函数没有意义
    TypeItem typeDesc;
    u64 realTypeName;
    u64 gcMark;//gcMark标记
    bool isNativeResouce = false;//是否native资源
    NativeResourceType nativeType;//本地资源类型
    u64 wrapType;//用于函数类型,包裹类在typeTable中的类型
    u64 text;//用于函数类型
    char data[0];//0长数组sizeof不占用空间(代码中用到了这个特性),对于函数对象，这个obj的内容是包裹类
};
#endif // !_HEAP
