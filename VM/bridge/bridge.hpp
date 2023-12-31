﻿//目前外部函数不应该返回HeapItem指针，因为外部返回的数据无法被GC管理，后续可以考虑由VM提供new函数

//这个头文件由VM向外部提供,创建自己的dll时可以引用此文件
#include "cstdint"
#ifndef _BRIDGE
#define _BRIDGE
namespace VMStaticExport {
	typedef int8_t i8;
	typedef int16_t i16;
	typedef int32_t i32;
	typedef int64_t i64;
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	typedef int8_t tbyte;
	typedef int16_t tshort;
	typedef int32_t tint;
	typedef int64_t tlong;
	typedef double tdouble;
	typedef uint64_t tpointer;

	enum typeItemDesc : uint64_t
	{
		PlaintObj = 0,
		Array,
		Function
	};
	struct TypeItem
	{
		typeItemDesc desc; //这个类型是数组、函数还是普通对象
		uint64_t innerType;     //对于array是数组元素类型在TypeTable中的位置，对于plainObj是classTable的类型，对于function则表示函数签名对应的类型(即在typeTable中的位置)
		uint64_t name;
	};

	struct HeapItem
	{
		union SizeOrLength {
			uint64_t size;//对于plainObj是size
			uint64_t length;//对于数组是length
		} sol;//对于函数没有意义
		TypeItem typeDesc;
		uint64_t realTypeName;
		uint64_t gcMark;//gcMark标记
		bool isNativeResouce = false;//是否native资源
		uint64_t NativeResourceFreeCallBack;//本地资源类型释放回调
		uint64_t wrapType;//用于函数类型,包裹类在typeTable中的类型
		uint64_t text;//用于函数类型
		char data[0];//0长数组sizeof不占用空间(代码中用到了这个特性),对于函数对象，这个obj的内容是包裹类
	};

	struct VM
	{
		//不提供创建数组的方法，因为这个数组类型也许在类型表里面还没有注册,比如int[]，假设源码没有出现过，那么typeTable和classTable都不会有这个类型，要创建的话还需要修改这两个表，太麻烦了
		tpointer(*addNativeResourcePointer)(tlong p, tlong freeCB);
	};
}
#endif // !_BRIDGE
