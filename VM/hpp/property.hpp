#ifndef _PROPERTY
#define _PROPERTY
#include "./environment.hpp"
struct PropertyItem
{
    u64 name;//属性名字
    u64 type;//属性在typeTable的描述符
};
struct PropertyDesc
{
    u64 size;//当前class的size
    u64 name;//当前class的name
    u64 isVALUE;//当前class是否为值类型
    u64 length;//属性数量
    PropertyItem *items;
};
#endif