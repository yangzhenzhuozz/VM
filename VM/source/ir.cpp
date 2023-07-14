#include <fstream>
#include <iostream>
#include "../hpp/ir.hpp"
IRs::IRs(const char* filename)
{
    std::ifstream fin(filename, std::ios_base::binary);
    if (!fin.is_open())
    {
        std::cout << "打开文件失败" << filename << std::endl;
    }
    fin.seekg(0, fin.end);
    u64 pos = fin.tellg();
    fin.seekg(0, fin.beg);

    fin.read((char*)&magicNumber, sizeof(u64));
    fin.read((char*)&_start, sizeof(u64));
    fin.read((char*)&_unwind, sizeof(u64));
    fin.read((char*)&VMThrow, sizeof(u64));
    fin.read((char*)&VMExceptionGen, sizeof(u64));
    fin.read((char*)&NullPointerException_init, sizeof(u64));
    fin.read((char*)&NullPointerException_constructor, sizeof(u64));
    fin.read((char*)&ArithmeticException_init, sizeof(u64));
    fin.read((char*)&ArithmeticException_constructor, sizeof(u64));
    fin.read((char*)&CastException_init, sizeof(u64));
    fin.read((char*)&CastException_constructor, sizeof(u64));
    fin.read((char*)&ArrayIndexOutOfBoundsException_init, sizeof(u64));
    fin.read((char*)&ArrayIndexOutOfBoundsException_constructor, sizeof(u64));

    length = (pos - sizeof(u64) * 7) / sizeof(IR);//前面有7个u64内容不是ir,有其他用处
    items = (IR*)new char[length * sizeof(IR)]; //申请内存

    fin.read((char*)items, length * sizeof(IR));
    if (length < 0)
    {
        //数据溢出
        delete items;
        items = nullptr;
    }
}

IRs::~IRs()
{
    delete[] items;
}