#include <fstream>
#include <iostream>
#include "../hpp/symbolTable.hpp"
SymbolTable::SymbolTable(const char* filename)
{
    std::ifstream fin(filename, std::ios_base::binary);
    if (!fin.is_open())
    {
        std::cout << "打开文件失败" << filename << std::endl;
    }
    fin.seekg(0, fin.end);
    auto pos = fin.tellg();
    fin.seekg(0, fin.beg);
    items = (symbolTableItem*)new char[pos]; //申请内存
    fin.read((char*)items, pos);
    length = pos / sizeof(symbolTableItem);
}

SymbolTable::~SymbolTable()
{
    delete[] items;
}