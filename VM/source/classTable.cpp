#include <fstream>
#include <iostream>
#include "../hpp/classTable.hpp"
ClassTable::ClassTable(const char* filename, StringPool& stringPool)
{
    std::ifstream fin(filename, std::ios_base::binary);
    if (!fin.is_open())
    {
        std::cout << "打开文件失败" << filename << std::endl;
    }
    fin.seekg(0, fin.end);
    auto pos = fin.tellg();
    fin.seekg(0, fin.beg);
    buffer = new char[pos]; //申请内存
    fin.read(buffer, pos);
    length = *((u64*)buffer);
    items = (PropertyDesc**)((u64)buffer + 8);
    //修正偏移
    for (u64 i = 0; i < length; i++)
    {
        ((u64*)items)[i] = ((u64*)items)[i] + (u64)buffer;
        if (items[i]->length > 0)
        {
            items[i]->items = (PropertyItem*)((u64)buffer + (u64)(items[i]->items));
        }
        auto typeName = stringPool.items[items[i]->name];//warning C6385好像避免不了
        if (strcmp(typeName, "system.bool") == 0)
        {
            system_bool = i;
        }
        else if (strcmp(typeName, "system.byte") == 0)
        {
            system_byte = i;
        }
        else if (strcmp(typeName, "system.short") == 0)
        {
            system_short = i;
        }
        else if (strcmp(typeName, "system.int") == 0)
        {
            system_int = i;
        }
        else if (strcmp(typeName, "system.long") == 0)
        {
            system_long = i;
        }
        else if (strcmp(typeName, "system.double") == 0)
        {
            system_double = i;
        }
        else if (strcmp(typeName, "system.object") == 0)
        {
            system_object = i;
        }
    }
}
ClassTable::~ClassTable()
{
    delete[] buffer;
}