#ifndef _TYPETABLE
#define _TYPETABLE
#include "./environment.hpp"
#include "./stringPool.hpp"
enum typeItemDesc : u64
{
    PlaintObj = 0,
    Array,
    Function
};
struct TypeItem
{
    typeItemDesc desc; //这个类型是数组、函数还是普通对象
    u64 innerType;     //对于array是数组元素类型在TypeTable中的位置，对于plainObj是classTable的类型，对于function则表示函数签名对应的类型(即在typeTable中的位置,在类型转换时可以判断是否允许转换)
    u64 name;
};

class TypeTable
{
public:
    u64 length;
    TypeItem* items;
    i64 system_exception_NullPointerException = -1;
    i64 system_exception_ArithmeticException = -1;
    i64 system_exception_CastException = -1;
    i64 system_exception_ArrayIndexOutOfBoundsException = -1;
    i64 system_bool = -1;
    i64 system_byte = -1;
    i64 system_short = -1;
    i64 system_int = -1;
    i64 system_long = -1;
    i64 system_double = -1;
    i64 system_object = -1;
    TypeTable(const char* filename, StringPool& stringPool);
    ~TypeTable();
};
#endif