#include <fstream>
#include <iostream>
#include "../hpp/nativeTable.hpp"

NativeTable::NativeTable(const char* filename, StringPool& stringPool)
{
    std::ifstream fin(filename, std::ios_base::binary);
    if (!fin.is_open())
    {
        std::cout << "打开文件失败" << filename << std::endl;
    }
    fin.seekg(0, fin.end);
    auto pos = fin.tellg();
    fin.seekg(0, fin.beg);
    u64 itemIndex = 0;
    for (;;)
    {
        u64 name;
        fin.read((char*)&name, sizeof(u64));
        u64 retSize;
        fin.read((char*)&retSize, sizeof(u64));
        u64 resultIsValueType;
        fin.read((char*)&resultIsValueType, sizeof(u64));
        u64 arglength;
        fin.read((char*)&arglength, sizeof(u64));
        NativeTableItem item;
        item.name = name;
        item.resultIsValueType = resultIsValueType;
        item.retSize = retSize;
        for (u64 i = 0; i < arglength; i++)
        {
            NativeArgumentItem argItem;
            fin.read((char*)&(argItem.size), sizeof(u64));
            fin.read((char*)&(argItem.isValueType), sizeof(u64));
            item.argList.push_back(argItem);
        }
        items.push_back(item);
        if (strcmp("system_loadLibrary", stringPool.items[name]) == 0)
        {
            system_loadLibrary = itemIndex;
        }
        else if (strcmp("system_fork", stringPool.items[name]) == 0) 
        {
            system_fork = itemIndex;
        }
        else if (strcmp("system_yield", stringPool.items[name]) == 0)
        {
            system_yield = itemIndex;
        }
        else if (strcmp("system_getCurrentThread", stringPool.items[name]) == 0)
        {
            system_getCurrentThread = itemIndex;
        }
        else if (strcmp("system_setCurrentThread", stringPool.items[name]) == 0)
        {
            system_setCurrentThread = itemIndex;
        }
        else if (strcmp("system_allocMutex", stringPool.items[name]) == 0)
        {
            system_allocMutex = itemIndex;
        }
        nativeMap[stringPool.items[name]] = itemIndex;
        itemIndex++;
        if (fin.tellg() == pos) {
            break;
        }
    }
}

NativeTable::~NativeTable()
{
}
