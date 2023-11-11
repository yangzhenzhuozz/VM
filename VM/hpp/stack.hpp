#ifndef _STACK
#define _STACK
#include "./environment.hpp"
class Stack
{
    u64 bp;
    u64 sp;
    char* buffer;
    const size_t limit = 1024 * 1024 * 2;

public:
    Stack();
    void push(char* data, u64 size);
    void push(u8);
    void push(u16);
    void push(u32);
    void push(u64);
    void push(i8);
    void push(i16);
    void push(i32);
    void push(i64);
    void push(double);
    void* pop(u64 size);
    u8 pop8();
    u16 pop16();
    u32 pop32();
    u64 pop64();
    double pop_double();
    u64 top64();
    u64 getBP();
    u64 getSP();
    void setData(char* data, u64 offsset, u64 size);
    void setBP(u64 v);
    void setSP(u64 v);
    void* getDataAdder(u64 offset);
    void* getDataAdderTop(u64 offset);//从栈顶往下取地址
    char* getBufferAddress();
    ~Stack();
};
#endif