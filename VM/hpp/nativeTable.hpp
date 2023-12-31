#ifndef _NATIVETABLE
#define _NATIVETABLE
#include "./environment.hpp"
#include "../hpp/stringPool.hpp"
#include <string>
#include <vector>
#include <map>
struct NativeArgumentItem
{
	u64 size;//参数size
	u64 isValueType;//是否值类型
};
class NativeTableItem
{
public:
	u64 name = 0;
	u64 retSize = 0;
	u64 resultIsValueType = 0;
	std::vector<NativeArgumentItem> argList;
	u64 realAddress = 0;//真实函数地址或者句柄
};
class NativeTable
{
public:
	std::map<std::string, u64> nativeMap;//本地函数映射表

	u64 system_VMLoadNativeLib = (u64)-1;//VM内置的一个native函数
	u64 system_fork = (u64)-1;//VM内置的一个native函数
	u64 system_yield = (u64)-1;//VM内置的一个native函数
	u64 system_getCurrentThread = (u64)-1;//VM内置的一个native函数
	u64 system_setCurrentThread = (u64)-1;//VM内置的一个native函数

	std::vector<NativeTableItem> items;
	NativeTable(const char* filename, StringPool& stringPool);
	~NativeTable();
};
#endif // !_NATIVETABLE
