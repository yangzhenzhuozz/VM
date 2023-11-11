#include "../hpp/stack.hpp"
#include <cstring>
#include <iostream>

Stack::Stack() :bp(0), sp(0)
{
	buffer = new char[limit];
}
void Stack::push(char* data, u64 size)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	memcpy((char*)((u64)buffer + sp), data, size);
	sp += size;
}
void Stack::push(u8 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(u8*)((u64)buffer + sp) = num;
	sp += sizeof(u8);
}
void Stack::push(u16 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(u16*)((u64)buffer + sp) = num;
	sp += sizeof(u16);
}
void Stack::push(u32 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(u32*)((u64)buffer + sp) = num;
	sp += sizeof(u32);
}
void Stack::push(u64 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(u64*)((u64)buffer + sp) = num;
	sp += sizeof(u64);
}

void Stack::push(i8 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(i8*)((u64)buffer + sp) = num;
	sp += sizeof(i8);
}
void Stack::push(i16 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(i16*)((u64)buffer + sp) = num;
	sp += sizeof(i16);
}
void Stack::push(i32 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(i32*)((u64)buffer + sp) = num;
	sp += sizeof(i32);
}
void Stack::push(i64 num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(i64*)((u64)buffer + sp) = num;
	sp += sizeof(i64);
}

void Stack::push(double num)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	*(double*)((u64)buffer + sp) = num;
	sp += sizeof(double);
}
void* Stack::pop(u64 size)
{
	sp = sp - size;
	return (char*)((u64)buffer + sp);
}
u8 Stack::pop8()
{
	sp = sp - sizeof(u8);
	return *((u8*)((u64)buffer + sp));
}

u16 Stack::pop16()
{
	sp = sp - sizeof(u16);
	return *((u16*)((u64)buffer + sp));
}

u32 Stack::pop32()
{
	sp = sp - sizeof(u32);
	return *((u32*)((u64)buffer + sp));
}
u64 Stack::pop64()
{
	sp = sp - sizeof(u64);
	return *((u64*)((u64)buffer + sp));
}

double Stack::pop_double()
{
	sp = sp - sizeof(double);
	return *((double*)((u64)buffer + sp));
}

u64 Stack::top64()
{
	return  *((u64*)((u64)buffer + sp - sizeof(u64)));
}
u64 Stack::getBP()
{
	return bp;
}
u64 Stack::getSP()
{
	return sp;
}
void Stack::setData(char* data, u64 offsset, u64 size)
{
	if (bp >= limit)
	{
		std::cerr << "stackoverflow" << std::endl;
		throw "stackoverflow";
	}
	char* dest = (char*)((u64)buffer + bp + offsset);
	memcpy(dest, data, size);
}
void Stack::setBP(u64 v)
{
	bp = v;
}
void Stack::setSP(u64 v)
{
	sp = v;
}
void* Stack::getDataAdder(u64 offset)
{
	return (void*)((u64)buffer + bp + offset);
}
void* Stack::getDataAdderTop(u64 offset)
{
	return (void*)((u64)buffer + sp - offset);
}
char* Stack::getBufferAddress()
{
	return buffer;
}
Stack::~Stack()
{
	delete[] buffer;
}