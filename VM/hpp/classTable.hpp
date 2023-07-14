#ifndef _CLASSTABLE
#define _CLASSTABLE
#include "./property.hpp"
#include "./stringPool.hpp"
class ClassTable
{
private:
    char *buffer;

public:

    i64 system_bool = -1;
    i64 system_byte = -1;
    i64 system_short = -1;
    i64 system_int = -1;
    i64 system_long = -1;
    i64 system_double = -1;
    i64 system_object = -1;

    u64 length;//class的数量
    PropertyDesc **items;
    ClassTable(const char *filename, StringPool& stringPool);
    ~ClassTable();
};
#endif