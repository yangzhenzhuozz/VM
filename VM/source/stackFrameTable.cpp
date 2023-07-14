#include <fstream>
#include <iostream>
#include "../hpp/stackFrameTable.hpp"
StackFrameTable::StackFrameTable(const char* filename)
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

    items = (StackFrameItem**)((u64)buffer + 8);
    //修正偏移
    for (u64 i = 0; i < length; i++)
    {
        ((u64*)items)[i] = ((u64*)items)[i] + (u64)buffer;
        if (items[i]->length > 0)
        {
            items[i]->items = (PropertyItem*)((u64)buffer + (u64)(items[i]->items));
        }
    }
}
StackFrameTable::~StackFrameTable()
{
    delete[] buffer;
}