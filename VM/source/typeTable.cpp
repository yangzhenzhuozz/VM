#include <fstream>
#include <iostream>
#include "../hpp/typeTable.hpp"
TypeTable::TypeTable(const char* filename, StringPool& stringPool)
{
    std::ifstream fin(filename, std::ios_base::binary);
    if (!fin.is_open())
    {
        std::cout << "打开文件失败" << filename << std::endl;
    }
    fin.seekg(0, fin.end);
    auto pos = fin.tellg();
    fin.seekg(0, fin.beg);
    items = (TypeItem*)new char[pos]; //申请内存
    fin.read((char*)items, pos);
    length = pos / sizeof(TypeItem);
    for (int i = 0; i < length; i++)
    {
        auto typeName = stringPool.items[items[i].name];//warning C6385好像避免不了
        if (strcmp(typeName, "system.exception.NullPointerException") == 0)
        {
            system_exception_NullPointerException = i;
        }
        else if (strcmp(typeName, "system.exception.ArithmeticException") == 0)
        {
            system_exception_ArithmeticException = i;
        }
        else if (strcmp(typeName, "system.exception.CastException") == 0)
        {
            system_exception_CastException = i;
        }
        else if (strcmp(typeName, "system.exception.ArrayIndexOutOfBoundsException") == 0)
        {
            system_exception_ArrayIndexOutOfBoundsException = i;
        }
        else if (strcmp(typeName, "system.bool") == 0)
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

TypeTable::~TypeTable()
{
    delete[] items;
}