#include "../hpp/vm.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <ffi.h>

u64 VM::program;
std::list<HeapItem*> VM::heap;
i32 VM::gcCounter = 0;
int VM::GCcondition = 0;
std::set<VM*> VM::VMs;
std::mutex VM::waitGC;

StringPool* VM::stringPool;
ClassTable* VM::classTable;
StackFrameTable* VM::stackFrameTable;
SymbolTable* VM::symbolTable;
TypeTable* VM::typeTable;
IRs* VM::irs;
NativeTable* VM::nativeTable;

void forked(HeapItem* funObj)
{
	/*
	* fork的参数会被GC回收掉
	*/
	VM vm;
	vm.pc = funObj->text;
	vm.calculateStack.push((u64)((u64*)funObj->data));
	vm.run();
}

VM::VM() :
	varStack(Stack()),
	calculateStack(Stack()),
	callStack(Stack()),
	unwindHandler(Stack()),
	unwindNumStack(Stack())
{
	VMs.insert(this);
}
void VM::_NativeCall(u64 NativeIndex)
{
	std::list<char*> argumentsBuffer;
	u64 resultSize = nativeTable->items[NativeIndex].retSize;
	char errMsg[1024];
	char* resultBuffer = new char[resultSize];
	memset(resultBuffer, 0, resultSize);
	auto argLen = nativeTable->items[NativeIndex].argList.size();//参数个数
	//从计算栈中弹出参数
	for (u64 i = 0; i < argLen; i++)
	{
		u64 argSize = nativeTable->items[NativeIndex].argList[i].size;
		char* argBuf = new char[argSize];
		argumentsBuffer.push_back(argBuf);
		auto top = calculateStack.getSP();
		memcpy(argBuf, calculateStack.getBufferAddress() + top - argSize, argSize);
		calculateStack.setSP(calculateStack.getSP() - argSize);
	}

	if (NativeIndex == nativeTable->system_loadLibrary)
	{
		auto it = argumentsBuffer.begin();
		HeapItem* arg0 = (HeapItem*)(((u64*)(*it))[0] - sizeof(HeapItem));
		it++;
		HeapItem* arg1 = (HeapItem*)(((u64*)(*it))[0] - sizeof(HeapItem));
		char* fileName = new char[arg0->sol.length + 5];
		memcpy(fileName, arg0->data, arg0->sol.length);
		fileName[arg0->sol.length] = '\0';
		strcat_s(fileName, arg0->sol.length + 5, ".dll");
		auto handle = LoadLibrary(fileName);
		if (handle == nullptr)
		{
			snprintf(errMsg, sizeof(errMsg), "加载动态库:%s 失败", fileName);;
			throw errMsg;
		}
		for (u64 i = 0; i < arg1->sol.length; i++)
		{
			HeapItem* functionNameObj = (HeapItem*)((*(u64*)((u64)(arg1->data) + sizeof(u64) * i)) - sizeof(HeapItem));
			char* functionName = new char[functionNameObj->sol.length + 1];
			memcpy(functionName, functionNameObj->data, functionNameObj->sol.length);
			functionName[functionNameObj->sol.length] = '\0';
			auto functionPointer = GetProcAddress(handle, functionName);
			if (functionPointer == nullptr)
			{
				snprintf(errMsg, sizeof(errMsg), "加载函数:%s 失败", functionName);;
				throw errMsg;
			}
			if (nativeTable->nativeMap.count(functionName) != 0)//如果这个函数在源码中有定义
			{
				nativeTable->items[nativeTable->nativeMap[functionName]].realAddress = (u64)functionPointer;
			}
			delete[] functionName;
		}
		delete[] fileName;
	}
	else if (NativeIndex == nativeTable->system_fork)
	{
		auto it = argumentsBuffer.begin();
		HeapItem* arg0 = (HeapItem*)(((u64*)(*it))[0] - sizeof(HeapItem));
		std::thread newThread(forked, arg0);
		newThread.detach();//分离新线程
	}
	else
	{
		if (nativeTable->items[NativeIndex].realAddress == 0)
		{
			snprintf(errMsg, sizeof(errMsg), "本地函数:%s 不存在,请检查是否已经使用VMLoadNativeLib函数加载对应的动态链接库", stringPool->items[nativeTable->items[NativeIndex].name]);;
			throw errMsg;
		}
		else
		{
			char** args = new char* [argLen];//准备参数
			ffi_type** argTyeps = new ffi_type * [argLen];//准备参数类型
			auto it = argumentsBuffer.begin();
			for (int argInex = 0; argInex < argLen; it++)
			{
				args[argInex] = (*it);//放置参数地址
				argTyeps[argInex] = new ffi_type;
				(*(argTyeps[argInex])).size = nativeTable->items[NativeIndex].argList[argInex].size;//参数大小
				(*(argTyeps[argInex])).alignment = 1;//对齐
				(*(argTyeps[argInex])).type = FFI_TYPE_STRUCT;//按结构体传参
				(*(argTyeps[argInex])).elements = nullptr;//没有元素
				argInex++;
			}

			ffi_type retType;//返回值类型声明
			if (resultSize == 0)
			{
				retType = ffi_type_void;
			}
			else
			{
				retType.alignment = 1;
				retType.size = resultSize;
				retType.type = FFI_TYPE_STRUCT;
				retType.elements = nullptr;
			}

			ffi_cif cif;
			//根据参数和返回值类型，设置cif模板
			//if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)argLen, &retType, argTyeps) == FFI_OK)
			//{
			//	//使用cif函数签名信息，调用函数
			//	ffi_call(&cif, (void (*)(void)) nativeTable->items[NativeIndex].realAddress, resultBuffer, (void**)args);
			//}
			//else
			//{
			//	throw "FFI参数错误";
			//}


			//释放为ffi参数类型描述符申请的内存
			for (auto i = 0; i < argLen; i++)
			{
				delete argTyeps[i];
			}
			delete[] argTyeps;
			delete[] args;

		}
	}

	//写回计算结果
	memcpy(calculateStack.getBufferAddress() + calculateStack.getSP(), resultBuffer, resultSize);
	calculateStack.setSP(calculateStack.getSP() + resultSize);

	//释放参数缓存
	for (auto it = argumentsBuffer.begin(); it != argumentsBuffer.end(); it++)
	{
		delete[] * it;
	}
	//如果返回的是一个引用类型，则将其录入heap中，否则释放内存
	if (nativeTable->items[NativeIndex].resultIsValueType != 1)
	{
		((HeapItem*)resultBuffer)->gcMark = gcCounter - 1;
		heap.push_back((HeapItem*)resultBuffer);
	}
	else
	{
		delete[] resultBuffer;
	}
}


/*
* 有空改成非递归算法，递归在层次多的时候确实挺慢的
*/
u64 VM::newArray(u64 arrayType, u32* param, u64 paramLen, u64 level)
{
	HeapItem* heapitem = nullptr;
	auto& typeDesc = typeTable->items[arrayType];
	auto elementType = typeDesc.innerType;
	//如果元素是值类型
	if (typeTable->items[elementType].desc == typeItemDesc::PlaintObj && classTable->items[typeTable->items[elementType].innerType]->isVALUE != 0)
	{
		heapitem = (HeapItem*) new char[sizeof(HeapItem) + classTable->items[typeTable->items[elementType].innerType]->size * param[level]];
		memset(heapitem->data, 0, classTable->items[typeTable->items[elementType].innerType]->size * param[level]);
	}
	else
	{
		heapitem = (HeapItem*)new char[sizeof(HeapItem) + sizeof(u64) * param[level]];
		//如果元素不是数组，则全部作为指针处理
		if (typeTable->items[elementType].desc != typeItemDesc::Array)
		{
			memset(heapitem->data, 0, sizeof(u64) * param[level]);
		}
		else
		{
			//如果是数组没有完整初始化,如new int[1][];则到当前层级已经结束
			if (paramLen - 1 == level)
			{
				memset(heapitem->data, 0, sizeof(u64) * param[level]);
			}
			else
			{
				for (u64 i = 0; i < param[level]; i++)
				{
					*((u64*)(heapitem->data) + i) = newArray(elementType, param, paramLen, level + 1);
				}
			}
		}
	}
	heapitem->sol.length = param[level];
	heapitem->typeDesc = typeDesc;
	heapitem->realTypeName = typeDesc.name;
	heapitem->gcMark = gcCounter - 1;
	heap.push_back(heapitem);
	return (u64)heapitem->data;
}

void VM::pop_stack_map(u64 level, bool isThrowPopup)
{
	for (auto i = 0; i < level; i++) {
		FrameItem frameItem = frameStack.back();
		frameStack.pop_back();
		auto isTryBlock = frameItem.isTryBlock;

		if (!isThrowPopup)
		{
			//只有正常的pop_stack_map指令需要检查，由throw导致的栈回退不需要检查isTryBlock
			if (isTryBlock != 0)
			{
				//如果是catch块的frame,则弹出
				catchStack.pop();
			}
		}

		auto frameIndex = frameItem.frameIndex;//frame

		auto& frame = stackFrameTable->items[frameIndex];
		//需要回退栈，判断当前已经分配了多少变量(比如异常就可能导致变量还未分配和初始化,把已经初始化的并且需要自动回退的变量处理掉)
		if (frame->autoUnwinding > 0)
		{
			auto varStackoffset = stackFrameTable->items[frameItem.frameIndex]->baseOffset;
			auto needUnwinded = 0;//计算需要弹出多少个unwind
			u64 unwindNum = 0;
			for (auto i = 0; i < frame->autoUnwinding; i++)
			{
				u64 size = 0;
				//如果是引用类型，则size等于8
				if (!classTable->items[typeTable->items[frame->items[i].type].innerType]->isVALUE)
				{
					size = 8;
				}
				else
				{
					size = classTable->items[typeTable->items[frame->items[i].type].innerType]->size;
				}

				if (varStackoffset < frameItem.frameSP)
				{
					unwindNum++;
					varStackoffset += size;
				}
				else//剩下的变量还没有分配
				{
					break;
				}
			}
			if (unwindNum > 0)
			{
				unwindNumStack.push(unwindNum);
				//开始执行@unwind
				callStack.push(pc);
				pc = irs->_unwind - 1;
				varStack.setBP(varStack.getSP());
			}
		}

		varStack.setBP(frameItem.lastBP);//回退BP
		varStack.setSP(varStack.getSP() - stackFrameTable->items[frameItem.frameIndex]->size);//回退上一帧的SP

	}
}

void VM::_new(u64 type)
{
	auto typeIndex = type;
	auto& typeDesc = typeTable->items[typeIndex];
	auto  name = typeDesc.name;
	auto dataSize = classTable->items[typeDesc.innerType]->size;
	if (classTable->items[typeDesc.innerType]->isVALUE == 0)
	{
		HeapItem* heapitem = (HeapItem*)new char[sizeof(HeapItem) + dataSize];
		memset(heapitem->data, 0, dataSize);
		heapitem->typeDesc = typeDesc;
		heapitem->sol.size = dataSize;
		heapitem->realTypeName = typeIndex;
		calculateStack.push((u64)heapitem->data);
		heapitem->gcMark = gcCounter - 1;
		heap.push_back(heapitem);
	}
	else
	{
		throw "value type cann't new";
	}
}

void VM::_throw(u64 type)
{
	for (;;)//依次弹出catch块
	{
		if (catchStack.empty())
		{
			char msgdBuf[1024];
			snprintf(msgdBuf, sizeof(msgdBuf), "unfind catch block match the type : %s", stringPool->items[typeTable->items[type].name]);//vm级别错误
			throw msgdBuf;
		}
		Catch_point catch_point = catchStack.top();
		catchStack.pop();
		for (auto it = catch_point.type_list.begin(); it != catch_point.type_list.end(); it++)
		{
			if (it->type == type)//类型匹配则进入异常处理程序
			{
				pc = it->irAddress - 1;//设置PC指针
				callStack.setSP(catch_point.callStackSP);//回退调用栈
				u64 frameLeve = frameStack.size() - catch_point.frameLevel;
				pop_stack_map(frameLeve, true);
				return;
			}
		}
	}
}

void VM::_VMThrowError(u64 type, u64 init, u64 constructor)
{
	if (VMError)
	{
		throw "双重错误";
	}
	calculateStack.setSP(0);//清空计算栈
	_new(type);//为空指针异常对象申请内存

	callStack.push(irs->VMThrow - 1);//使异常构造函数结束之后返回到VMThrow
	varStack.setBP(varStack.getSP());

	pc = irs->VMExceptionGen - 1;

	irs->items[irs->VMExceptionGen].operand1 = init;//修改ir，使其调用init
	irs->items[irs->VMExceptionGen + 1].operand1 = constructor;//修改ir，使其调用构造函数

	irs->items[irs->VMThrow + 2].operand1 = type;//正确的抛出空指针异常

	VMError = true;
}

void VM::run()
{
	/*
	* 比如系统触发了一个NullPointException，然后在构造NullPointException的时候又触发异常，直接GG，根本无法抢救
	* 这里指的都是VM自身产生的异常，用户代码产生的异常不在此列
	*/
	try {
		for (; pc < irs->length; pc++)
		{
			auto& ir = irs->items[pc];
			switch (ir.opcode)
			{
			case OPCODE::_new:
			{
				_new(ir.operand1);
			}
			break;
			case OPCODE::newFunc:
			{
				auto wrapIndex = ir.operand3;
				auto& typeDesc = typeTable->items[wrapIndex];
				auto name = typeDesc.name;
				auto dataSize = classTable->items[typeDesc.innerType]->size;
				HeapItem* heapitem = (HeapItem*)new char[sizeof(HeapItem) + dataSize];
				memset(heapitem->data, 0, dataSize);
				heapitem->typeDesc = typeDesc;
				heapitem->sol.size = dataSize;
				heapitem->realTypeName = ir.operand2;
				heapitem->wrapType = wrapIndex;
				heapitem->text = ir.operand1;
				calculateStack.push((u64)heapitem->data);
				heapitem->gcMark = gcCounter - 1;
				heap.push_back(heapitem);
				if (heapitem->text == 0)
				{
					throw "error";
				}
			}
			break;
			case OPCODE::newArray:
			{
				auto arrayType = ir.operand1;
				auto paramLen = ir.operand2;
				u32* param = (u32*)calculateStack.pop(sizeof(i32) * paramLen);
				u64 arrayAddress = newArray(arrayType, param, paramLen, 0);
				calculateStack.push(arrayAddress);
			}
			break;
			case OPCODE::program_load:
			{
				calculateStack.push(program);
			}
			break;
			case OPCODE::program_store:
			{
				program = calculateStack.pop64();
			}
			break;
			case OPCODE::p_getfield:
			{
				u64 baseObj = calculateStack.pop64();
				if (baseObj == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					calculateStack.push(*(u64*)(baseObj + ir.operand1));
				}
			}
			break;
			case OPCODE::p_putfield:
			{
				u64 value = calculateStack.pop64();
				u64 targetObj = calculateStack.pop64();
				if (targetObj == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					*(u64*)(targetObj + ir.operand1) = value;
				}
			}
			break;
			case OPCODE::valueType_load:
			{
				calculateStack.push((char*)varStack.getDataAdder(ir.operand1), ir.operand2);
			}
			break;
			case OPCODE::valueType_store:
			{
				char* data = (char*)calculateStack.pop(ir.operand2);
				varStack.setData(data, ir.operand1, ir.operand2);
			}
			break;
			case OPCODE::init_valueType_store:
			{
				char* data = (char*)calculateStack.pop(ir.operand2);
				varStack.setData(data, ir.operand1, ir.operand2);
				frameStack.back().frameSP = frameStack.back().frameSP + ir.operand2;
			}
			break;
			case OPCODE::p_load:
			{
				auto val = *((u64*)varStack.getDataAdder(ir.operand1));
				calculateStack.push(val);
			}
			break;
			case OPCODE::p_store:
			{
				auto val = calculateStack.pop64();
				varStack.setData((char*)&val, ir.operand1, sizeof(u64));
			}
			break;
			case OPCODE::init_p_store:
			{
				auto val = calculateStack.pop64();
				varStack.setData((char*)&val, ir.operand1, sizeof(u64));
				frameStack.back().frameSP = frameStack.back().frameSP + sizeof(u64);
			}
			break;


			case OPCODE::i8_shl:
			{
				i32 v2 = calculateStack.pop32();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 << v2));
			}
			break;
			case OPCODE::i8_shr:
			{
				i32 v2 = calculateStack.pop32();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 >> v2));
			}
			break;
			case OPCODE::i8_or:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 | v2));
			}
			break;
			case OPCODE::i8_and:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 & v2));
			}
			break;
			case OPCODE::i8_xor:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 ^ v2));
			}
			break;
			case OPCODE::i8_mod:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 % v2));
			}
			break;
			case OPCODE::i8_add:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 + v2));
			}
			break;
			case OPCODE::i8_sub:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 - v2));
			}
			break;
			case OPCODE::i8_mul:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				calculateStack.push((i8)(v1 * v2));
			}
			break;
			case OPCODE::i8_div:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v2 == 0)
				{
					_VMThrowError(typeTable->system_exception_ArithmeticException, irs->ArithmeticException_init, irs->ArithmeticException_constructor);
				}
				else
				{
					calculateStack.push((i8)(v1 / v2));
				}
			}
			break;
			case OPCODE::i8_inc:
			{
				i8* address = (i8*)calculateStack.getDataAdderTop(sizeof(i8));
				(*address)++;
			}
			break;
			case OPCODE::i8_dec:
			{
				i8* address = (i8*)calculateStack.getDataAdderTop(sizeof(i8));
				(*address)--;
			}
			break;
			case OPCODE::i8_not:
			{
				i8* address = (i8*)calculateStack.getDataAdderTop(sizeof(i8));
				(*address) = ~(*address);
			}
			break;
			case OPCODE::i8_negative:
			{
				i8* address = (i8*)calculateStack.getDataAdderTop(sizeof(i8));
				(*address) = -(*address);
			}
			break;



			case OPCODE::i16_shl:
			{
				i32 v2 = calculateStack.pop32();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 << v2));
			}
			break;
			case OPCODE::i16_shr:
			{
				i32 v2 = calculateStack.pop32();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 >> v2));
			}
			break;
			case OPCODE::i16_or:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 | v2));
			}
			break;
			case OPCODE::i16_and:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 & v2));
			}
			break;
			case OPCODE::i16_xor:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 ^ v2));
			}
			break;
			case OPCODE::i16_mod:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 % v2));
			}
			break;
			case OPCODE::i16_add:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 + v2));
			}
			break;
			case OPCODE::i16_sub:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 - v2));
			}
			break;
			case OPCODE::i16_mul:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				calculateStack.push((i16)(v1 * v2));
			}
			break;
			case OPCODE::i16_div:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v2 == 0)
				{
					_VMThrowError(typeTable->system_exception_ArithmeticException, irs->ArithmeticException_init, irs->ArithmeticException_constructor);
				}
				else
				{
					calculateStack.push((i16)(v1 / v2));
				}
			}
			break;
			case OPCODE::i16_inc:
			{
				i16* address = (i16*)calculateStack.getDataAdderTop(sizeof(i16));
				(*address)++;
			}
			break;
			case OPCODE::i16_dec:
			{
				i16* address = (i16*)calculateStack.getDataAdderTop(sizeof(i16));
				(*address)--;
			}
			break;
			case OPCODE::i16_not:
			{
				i16* address = (i16*)calculateStack.getDataAdderTop(sizeof(i16));
				(*address) = ~(*address);
			}
			break;
			case OPCODE::i16_negative:
			{
				i16* address = (i16*)calculateStack.getDataAdderTop(sizeof(i16));
				(*address) = -(*address);
			}
			break;



			case OPCODE::i32_shl:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 << v2));
			}
			break;
			case OPCODE::i32_shr:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 >> v2));
			}
			break;
			case OPCODE::i32_or:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 | v2));
			}
			break;
			case OPCODE::i32_and:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 & v2));
			}
			break;
			case OPCODE::i32_xor:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 ^ v2));
			}
			break;
			case OPCODE::i32_mod:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 % v2));
			}
			break;
			case OPCODE::i32_add:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 + v2));
			}
			break;
			case OPCODE::i32_sub:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 - v2));
			}
			break;
			case OPCODE::i32_mul:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				calculateStack.push((i32)(v1 * v2));
			}
			break;
			case OPCODE::i32_div:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v2 == 0)
				{
					_VMThrowError(typeTable->system_exception_ArithmeticException, irs->ArithmeticException_init, irs->ArithmeticException_constructor);
				}
				else
				{
					calculateStack.push((i32)(v1 / v2));
				}
			}
			break;
			case OPCODE::i32_inc:
			{
				i32* address = (i32*)calculateStack.getDataAdderTop(sizeof(i32));
				(*address)++;
			}
			break;
			case OPCODE::i32_dec:
			{
				i32* address = (i32*)calculateStack.getDataAdderTop(sizeof(i32));
				(*address)--;
			}
			break;
			case OPCODE::i32_not:
			{
				i32* address = (i32*)calculateStack.getDataAdderTop(sizeof(i32));
				(*address) = ~(*address);
			}
			break;
			case OPCODE::i32_negative:
			{
				i32* address = (i32*)calculateStack.getDataAdderTop(sizeof(i32));
				(*address) = -(*address);
			}
			break;



			case OPCODE::i64_shl:
			{
				i32 v2 = calculateStack.pop32();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 << v2));
			}
			break;
			case OPCODE::i64_shr:
			{
				i32 v2 = calculateStack.pop32();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 >> v2));
			}
			break;
			case OPCODE::i64_or:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 | v2));
			}
			break;
			case OPCODE::i64_and:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 & v2));
			}
			break;
			case OPCODE::i64_xor:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 ^ v2));
			}
			break;
			case OPCODE::i64_mod:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 % v2));
			}
			break;
			case OPCODE::i64_add:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 + v2));
			}
			break;
			case OPCODE::i64_sub:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 - v2));
			}
			break;
			case OPCODE::i64_mul:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				calculateStack.push((i64)(v1 * v2));
			}
			break;
			case OPCODE::i64_div:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v2 == 0)
				{
					_VMThrowError(typeTable->system_exception_ArithmeticException, irs->ArithmeticException_init, irs->ArithmeticException_constructor);
				}
				else
				{
					calculateStack.push((i64)(v1 / v2));
				}
			}
			break;
			case OPCODE::i64_inc:
			{
				i64* address = (i64*)calculateStack.getDataAdderTop(sizeof(i64));
				(*address)++;
			}
			break;
			case OPCODE::i64_dec:
			{
				i64* address = (i64*)calculateStack.getDataAdderTop(sizeof(i64));
				(*address)--;
			}
			break;
			case OPCODE::i64_not:
			{
				i64* address = (i64*)calculateStack.getDataAdderTop(sizeof(i64));
				(*address) = ~(*address);
			}
			break;
			case OPCODE::i64_negative:
			{
				i64* address = (i64*)calculateStack.getDataAdderTop(sizeof(i64));
				(*address) = -(*address);
			}
			break;


			case OPCODE::double_add:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				calculateStack.push((double)(v1 + v2));
			}
			break;
			case OPCODE::double_sub:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				calculateStack.push((double)(v1 - v2));
			}
			break;
			case OPCODE::double_mul:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				calculateStack.push((double)(v1 * v2));
			}
			break;
			case OPCODE::double_div:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				calculateStack.push((double)(v1 / v2));
			}
			break;
			case OPCODE::double_inc:
			{
				double* address = (double*)calculateStack.getDataAdderTop(sizeof(double));
				(*address)++;
			}
			break;
			case OPCODE::double_dec:
			{
				double* address = (double*)calculateStack.getDataAdderTop(sizeof(double));
				(*address)--;
			}
			break;
			case OPCODE::double_negative:
			{
				double* address = (double*)calculateStack.getDataAdderTop(sizeof(double));
				(*address) = -(*address);
			}
			break;



			case OPCODE::i8_if_gt:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 > v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_ge:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 >= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_lt:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 < v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_le:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 <= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_cmp_eq:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 == v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_cmp_ne:
			{
				i8 v2 = calculateStack.pop8();
				i8 v1 = calculateStack.pop8();
				if (v1 != v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;





			case OPCODE::i16_if_gt:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 > v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i16_if_ge:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 >= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i16_if_lt:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 < v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i16_if_le:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 <= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i16_if_cmp_eq:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 == v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i16_if_cmp_ne:
			{
				i16 v2 = calculateStack.pop16();
				i16 v1 = calculateStack.pop16();
				if (v1 != v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;

			case OPCODE::i32_if_gt:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 > v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i32_if_ge:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 >= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i32_if_lt:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 < v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i32_if_le:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 <= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i32_if_cmp_eq:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 == v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i32_if_cmp_ne:
			{
				i32 v2 = calculateStack.pop32();
				i32 v1 = calculateStack.pop32();
				if (v1 != v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;


			case OPCODE::i64_if_gt:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 > v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i64_if_ge:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 >= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i64_if_lt:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 < v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i64_if_le:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 <= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i64_if_cmp_eq:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 == v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i64_if_cmp_ne:
			{
				i64 v2 = calculateStack.pop64();
				i64 v1 = calculateStack.pop64();
				if (v1 != v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;


			case OPCODE::double_if_gt:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 > v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::double_if_ge:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 >= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::double_if_lt:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 < v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::double_if_le:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 <= v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::double_if_cmp_eq:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 == v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::double_if_cmp_ne:
			{
				double v2 = calculateStack.pop_double();
				double v1 = calculateStack.pop_double();
				if (v1 != v2)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;



			case OPCODE::i8_if_false:
			{
				auto v = calculateStack.pop8();
				if (v == 0)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;
			case OPCODE::i8_if_true:
			{
				auto v = calculateStack.pop8();
				if (v == 1)
				{
					pc += ir.operand1 - 1;
				}
			}
			break;



			case OPCODE::jmp:
			{
				pc += ir.operand1 - 1;
			}
			break;
			case OPCODE::p_dup:
			{
				calculateStack.push(calculateStack.top64());
			}
			break;
			case OPCODE::valueType_pop:
			{
				calculateStack.pop(ir.operand1);
			}
			break;
			case OPCODE::p_pop:
			{
				calculateStack.pop64();
			}
			break;
			case OPCODE::abs_call:
			{
				callStack.push(pc);
				pc = ir.operand1 - 1;//因为在for循环结束会自动加一
			}
			break;
			case OPCODE::call:
			{
				auto functionObj = calculateStack.top64();
				if (functionObj == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					callStack.push(pc);
					HeapItem* heapItem = (HeapItem*)(functionObj - sizeof(HeapItem));
					if (heapItem->text == 0)
					{
						throw "error";
					}
					pc = heapItem->text - 1;
				}
			}
			break;
			case OPCODE::alloc:
			{
				frameStack.back().frameSP = frameStack.back().frameSP + ir.operand1;
			}
			break;
			case OPCODE::alloc_null:
			{
				frameStack.back().frameSP = frameStack.back().frameSP + 8;
				memset((char*)((u64)varStack.getBufferAddress() + varStack.getBP() + frameStack.back().frameSP - ir.operand1), 0x00, 8);
			}
			break;
			case OPCODE::native_call:
			{
				calculateStack.pop64();//从计算栈中弹出函数对象
				_NativeCall(ir.operand1);
			}
			break;
			case OPCODE::const_i8_load:
			{
				calculateStack.push((u8)ir.operand1);
			}
			break;
			case OPCODE::const_i16_load:
			{
				calculateStack.push((u16)ir.operand1);
			}
			break;
			case OPCODE::const_i32_load:
			{
				calculateStack.push((u32)ir.operand1);
			}
			break;
			case OPCODE::const_i64_load:
			{
				calculateStack.push((u64)ir.operand1);
			}
			break;
			case OPCODE::const_double_load:
			{
				calculateStack.push((u64)ir.operand1);
			}
			break;
			case OPCODE::valueType_getfield:
			{
				u64 baseObj = calculateStack.pop64();
				if (baseObj == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					char* data = (char*)(baseObj + ir.operand1);
					calculateStack.push(data, ir.operand2);
				}
			}
			break;
			case OPCODE::valueType_putfield:
			{
				char* data = (char*)calculateStack.pop(ir.operand2);
				u64 targetObj = calculateStack.pop64();
				if (targetObj == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					memcpy((char*)(targetObj + ir.operand1), data, ir.operand2);
				}
			}
			break;
			case OPCODE::getfield_address:
			{
				auto baseAdd = calculateStack.pop64();
				if (baseAdd == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					auto add = baseAdd + ir.operand1;
					calculateStack.push(add);
				}
			}
			break;
			case OPCODE::load_address:
			{
				calculateStack.push((u64)varStack.getDataAdder(ir.operand1));
			}
			break;
			case OPCODE::array_get_element_address:
			{
				auto index = calculateStack.pop32();
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					if (index >= heapitem->sol.length)
					{
						_VMThrowError(typeTable->system_exception_ArrayIndexOutOfBoundsException, irs->ArrayIndexOutOfBoundsException_init, irs->ArrayIndexOutOfBoundsException_constructor);
					}
					else
					{
						calculateStack.push((u64)(arrayAddress + ir.operand1 * index));
					}
				}
			}
			break;
			case OPCODE::array_get_point:
			{
				auto index = calculateStack.pop32();
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					if (index >= heapitem->sol.length)
					{
						_VMThrowError(typeTable->system_exception_ArrayIndexOutOfBoundsException, irs->ArrayIndexOutOfBoundsException_init, irs->ArrayIndexOutOfBoundsException_constructor);
					}
					else
					{
						auto val = *(u64*)(arrayAddress + sizeof(u64) * index);
						calculateStack.push(val);
					}
				}
			}
			break;
			case OPCODE::array_get_valueType:
			{
				auto index = calculateStack.pop32();
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					if (index >= heapitem->sol.length)
					{
						_VMThrowError(typeTable->system_exception_ArrayIndexOutOfBoundsException, irs->ArrayIndexOutOfBoundsException_init, irs->ArrayIndexOutOfBoundsException_constructor);
					}
					else
					{
						char* data = (char*)(arrayAddress + ir.operand1 * index);
						calculateStack.push(data, ir.operand1);
					}
				}
			}
			break;
			case OPCODE::array_set_point:
			{
				auto val = calculateStack.pop64();
				auto index = calculateStack.pop32();
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					if (index >= heapitem->sol.length)
					{
						_VMThrowError(typeTable->system_exception_ArrayIndexOutOfBoundsException, irs->ArrayIndexOutOfBoundsException_init, irs->ArrayIndexOutOfBoundsException_constructor);
					}
					else
					{
						u64* dest = (u64*)(arrayAddress + sizeof(u64) * index);
						*dest = val;
					}
				}
			}
			break;
			case OPCODE::array_set_valueType:
			{
				char* valpoint = (char*)calculateStack.pop(ir.operand1);
				auto index = calculateStack.pop32();
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					if (index >= heapitem->sol.length)
					{
						_VMThrowError(typeTable->system_exception_ArrayIndexOutOfBoundsException, irs->ArrayIndexOutOfBoundsException_init, irs->ArrayIndexOutOfBoundsException_constructor);
					}
					else
					{
						memcpy((char*)(arrayAddress + ir.operand1 * index), valpoint, ir.operand1);
					}
				}
			}
			break;
			case OPCODE::access_array_length:
			{
				auto arrayAddress = calculateStack.pop64();
				if (arrayAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					HeapItem* heapitem = (HeapItem*)(arrayAddress - sizeof(HeapItem));
					calculateStack.push((u32)heapitem->sol.length);
				}
			}
			break;
			case OPCODE::box:
			{
				auto typeIndex = ir.operand1;
				auto& typeDesc = typeTable->items[typeIndex];
				auto  name = typeDesc.name;
				auto dataSize = classTable->items[typeDesc.innerType]->size;
				if (classTable->items[typeDesc.innerType]->isVALUE != 0)
				{
					HeapItem* heapitem = (HeapItem*)new char[sizeof(HeapItem) + dataSize];
					auto src = (char*)(calculateStack.getBufferAddress() + calculateStack.getSP() - dataSize);
					memcpy(heapitem->data, src, dataSize);
					heapitem->typeDesc = typeDesc;
					heapitem->sol.size = dataSize;
					heapitem->realTypeName = typeIndex;
					heapitem->gcMark = gcCounter - 1;
					heap.push_back(heapitem);

					calculateStack.setSP(calculateStack.getSP() - dataSize);
					calculateStack.push((u64)(heapitem->data));
				}
				else
				{
					throw "value type cann box only";
				}
			}
			break;
			case OPCODE::unbox:
			{
				HeapItem* heapItem = (HeapItem*)(calculateStack.pop64() - sizeof(HeapItem));
				TypeItem& srcTypeDesc = (*heapItem).typeDesc;
				TypeItem& targetTypeDesc = typeTable->items[ir.operand1];
				if (srcTypeDesc.desc != targetTypeDesc.desc || srcTypeDesc.innerType != targetTypeDesc.innerType || srcTypeDesc.name != targetTypeDesc.name)
				{
					_VMThrowError(typeTable->system_exception_CastException, irs->CastException_init, irs->CastException_constructor);
				}
				else
				{
					calculateStack.push(heapItem->data, heapItem->sol.size);
				}
			}
			break;
			case OPCODE::instanceof:
			{
				HeapItem* heapItem = (HeapItem*)(calculateStack.pop64() - sizeof(HeapItem));
				TypeItem& srcTypeDesc = (*heapItem).typeDesc;
				TypeItem& targetTypeDesc = typeTable->items[ir.operand1];
				if (srcTypeDesc.desc != targetTypeDesc.desc || srcTypeDesc.innerType != targetTypeDesc.innerType || srcTypeDesc.name != targetTypeDesc.name)
				{
					calculateStack.push((i8)0);
				}
				else
				{
					calculateStack.push((i8)1);
				}
			}
			break;
			case OPCODE::castCheck:
			{
				auto objAddress = calculateStack.top64();
				if (objAddress == 0)
				{
					_VMThrowError(typeTable->system_exception_NullPointerException, irs->NullPointerException_init, irs->NullPointerException_constructor);
				}
				else
				{
					TypeItem& srcTypeDesc = (*(HeapItem*)(objAddress - sizeof(HeapItem))).typeDesc;
					TypeItem& targetTypeDesc = typeTable->items[ir.operand1];
					if (srcTypeDesc.desc != targetTypeDesc.desc || srcTypeDesc.innerType != targetTypeDesc.innerType || srcTypeDesc.name != targetTypeDesc.name)
					{
						_VMThrowError(typeTable->system_exception_CastException, irs->CastException_init, irs->CastException_constructor);
					}
				}
			}
			break;

			case OPCODE::b2s: {i8 v = calculateStack.pop8(); calculateStack.push((i16)v); } break;
			case OPCODE::b2i: {i8 v = calculateStack.pop8(); calculateStack.push((i32)v); } break;
			case OPCODE::b2l: {i8 v = calculateStack.pop8(); calculateStack.push((i64)v); } break;
			case OPCODE::b2d: {i8 v = calculateStack.pop8(); calculateStack.push((double)v); } break;

			case OPCODE::s2b: {i16 v = calculateStack.pop16(); calculateStack.push((i8)v); } break;
			case OPCODE::s2i: {i16 v = calculateStack.pop16(); calculateStack.push((i32)v); } break;
			case OPCODE::s2l: {i16 v = calculateStack.pop16(); calculateStack.push((i64)v); } break;
			case OPCODE::s2d: {i16 v = calculateStack.pop16(); calculateStack.push((double)v); } break;

			case OPCODE::i2b: {i32 v = calculateStack.pop32(); calculateStack.push((i8)v); } break;
			case OPCODE::i2s: {i32 v = calculateStack.pop32(); calculateStack.push((i16)v); } break;
			case OPCODE::i2l: {i32 v = calculateStack.pop32(); calculateStack.push((i64)v); } break;
			case OPCODE::i2d: {i32 v = calculateStack.pop32(); calculateStack.push((double)v); } break;

			case OPCODE::l2b: {i64 v = calculateStack.pop64(); calculateStack.push((i8)v); } break;
			case OPCODE::l2s: {i64 v = calculateStack.pop64(); calculateStack.push((i16)v); } break;
			case OPCODE::l2i: {i64 v = calculateStack.pop64(); calculateStack.push((i32)v); } break;
			case OPCODE::l2d: {i64 v = calculateStack.pop64(); calculateStack.push((double)v); } break;

			case OPCODE::d2b: {double v = calculateStack.pop_double(); calculateStack.push((i8)v); } break;
			case OPCODE::d2s: {double v = calculateStack.pop_double(); calculateStack.push((i16)v); } break;
			case OPCODE::d2i: {double v = calculateStack.pop_double(); calculateStack.push((i32)v); } break;
			case OPCODE::d2l: {double v = calculateStack.pop_double(); calculateStack.push((i64)v); } break;

			case OPCODE::push_stack_map:
			{
				FrameItem item = { 0 };
				item.frameSP = stackFrameTable->items[ir.operand1]->baseOffset;
				item.lastBP = varStack.getBP();
				item.frameIndex = ir.operand1;
				item.isTryBlock = stackFrameTable->items[ir.operand1]->isTryBlock;
				//如果是函数block，则更新bp
				if (stackFrameTable->items[ir.operand1]->isFunctionBlock)
				{
					varStack.setBP(varStack.getSP());
				}
				//申请变量空间
				varStack.setSP(varStack.getSP() + stackFrameTable->items[ir.operand1]->size);

				frameStack.push_back(item);
			}
			break;
			case OPCODE::pop_stack_map:
			{
				pop_stack_map(ir.operand1, false);
			}
			break;

			case OPCODE::push_unwind:
			{
				auto point = calculateStack.pop64();
				unwindHandler.push(point);
			}
			break;
			case OPCODE::pop_unwind:
			{
				auto handler = unwindHandler.pop64();
				calculateStack.push(handler);
				unwindNumStack.push(unwindNumStack.pop64() - 1);
			}
			break;
			case OPCODE::if_unneed_unwind:
			{
				if (unwindNumStack.top64() == 0)
				{
					pc = pc + ir.operand1 - 1;
					unwindNumStack.pop64();
				}
			}
			break;

			case OPCODE::push_catch_block:
			{
				calculateStack.push(ir.operand1);
				calculateStack.push(ir.operand2);
			}
			break;
			case OPCODE::save_catch_point:
			{
				Catch_point point;
				for (u64 i = 0; i < ir.operand1; i++)
				{
					auto type = calculateStack.pop64();
					auto irAddress = calculateStack.pop64();//intermedial represent
					Catch_item item;
					item.type = type;
					item.irAddress = irAddress;
					point.type_list.push_back(item);
				}
				point.varBP = varStack.getBP();
				point.varSP = varStack.getSP();
				point.frameLevel = frameStack.size();
				point.callStackSP = callStack.getSP();
				catchStack.push(point);
			}
			break;
			case OPCODE::clear_calculate_stack:
			{
				calculateStack.setSP(0);
			}
			break;
			case OPCODE::_throw:
			{
				_throw(ir.operand1);
			}
			break;

			case OPCODE::clear_VM_Error_flag:
			{
				VMError = false;
			}
			break;

			case OPCODE::store_VM_Error:
			{
				auto error = calculateStack.pop64();
				calculateStack.setSP(0);
				calculateStack.push(error);
			}
			break;
			case OPCODE::ret:
			{
				if (callStack.getBP() == 0 && callStack.getSP() == 0)
				{
					setSafePoint();
					goto __exit;
				}
				else
				{
					pc = callStack.pop64();
				}
			}
			break;
			case OPCODE::__exit:
			{
				program = 0x00;
				setSafePoint();
				if (!heap.empty())
				{
					throw "GC没有回收全部对象";
				}
				goto __exit;
			}
			break;

			default:
			{
				throw "unimplement";
			}
			break;
			}
			if (calculateStack.getSP() == 0)//如果一行语句结束(计算栈没有内容)，则尝试进行GC
			{
				setSafePoint();
			}
		}
	}
	catch (char* err) {
		std::cerr << err << std::endl;
	}
	catch (const char* err) {
		std::cerr << err << std::endl;
	}
__exit:
	if (callStack.getBP() != 0 || callStack.getSP() != 0)
	{
		throw "栈不平衡";
	}
	if (calculateStack.getBP() != 0 || calculateStack.getSP() != 0)
	{
		throw "栈不平衡";
	}
	if (varStack.getBP() != 0 || varStack.getSP() != 0)
	{
		throw "栈不平衡";
	}
	if (frameStack.size() != 0)
	{
		throw "栈不平衡";
	}
	if (unwindNumStack.getBP() != 0 || unwindNumStack.getSP() != 0)
	{
		throw "栈不平衡";
	}
	if (unwindHandler.getBP() != 0 || unwindHandler.getSP() != 0)
	{
		throw "栈不平衡";
	}
}

void VM::sweep()
{
	auto garbageCounter = 0;
	for (auto it = heap.begin(); it != heap.end();)
	{
		if ((*it)->gcMark != gcCounter)
		{
			delete (*it);
			heap.erase(it++);//STL坑点之一
			garbageCounter++;
		}
		else
		{
			it++;
		}
	}
	//std::cout << "gc数量:" << garbageCounter << std::endl;
}


/*
* 分析一个对象，把内部的所有引用类型添加到GCRoots
* 这些指针已经把我搞蒙了
*/
void VM::GCClassFieldAnalyze(std::list<HeapItem*>& GCRoots, u64 dataAddress, u64 classIndex)
{
	//如果被扫描的类型是系统内置值类型，则不再扫描(除了object)
	if (
		classIndex == classTable->system_bool ||
		classIndex == classTable->system_byte ||
		classIndex == classTable->system_short ||
		classIndex == classTable->system_int ||
		classIndex == classTable->system_long ||
		classIndex == classTable->system_double
		)
	{
		return;
	}
	//遍历对象的所有属性
	u64 fieldOffset = 0;
	for (auto fieldIndex = 0; fieldIndex < classTable->items[classIndex]->length; fieldIndex++) {
		u64 fieldTypeTableIndex = classTable->items[classIndex]->items[fieldIndex].type;
		TypeItem& fieldTypeDesc = typeTable->items[fieldTypeTableIndex];
		if (fieldTypeDesc.desc == typeItemDesc::PlaintObj)//是class
		{
			if (classTable->items[fieldTypeDesc.innerType]->isVALUE)//是值类型
			{
				if (fieldTypeTableIndex == typeTable->system_object)//如果是object，则把他当作引用对待
				{
					u64 obj = *((u64*)(dataAddress + fieldOffset));
					if (obj != 0)//不是null
					{
						if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
						{
							auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
							GCClassFieldAnalyze(GCRoots, obj, realType);
						}
					}
				}
				else
				{
					GCClassFieldAnalyze(GCRoots, dataAddress + fieldOffset, fieldTypeDesc.innerType);
				}
				fieldOffset += classTable->items[fieldTypeDesc.innerType]->size;//偏移增加
			}
			else//是引用类型
			{
				u64 obj = *((u64*)(dataAddress + fieldOffset));
				if (obj != 0)//不是null
				{
					if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
					{
						GCClassFieldAnalyze(GCRoots, obj, fieldTypeDesc.innerType);
					}
				}
				fieldOffset += sizeof(u64);//偏移增加
			}
		}
		else if (fieldTypeDesc.desc == typeItemDesc::Array)//是数组
		{
			u64 arr = *((u64*)(dataAddress + fieldOffset));
			if (arr != 0) //数组不是null
			{
				if (mark(GCRoots, (HeapItem*)(arr - sizeof(HeapItem))))
				{
					GCArrayAnalyze(GCRoots, arr);
				}
			}
			fieldOffset += sizeof(u64);//偏移增加
		}
		else//是函数对象
		{
			/*
			* 函数对象和object一样，只是一个指针，指向了包裹类，所以需要用realType取得指向的那个包裹类类型
			*/
			u64 obj = *((u64*)(dataAddress + fieldOffset));
			if (obj != 0)//不是null
			{
				if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
				{
					auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
					GCClassFieldAnalyze(GCRoots, obj, realType);
				}
			}
			fieldOffset += sizeof(u64);//偏移增加
		}
	}
}
void VM::GCArrayAnalyze(std::list<HeapItem*>& GCRoots, u64 dataAddress)
{
	HeapItem* array = (HeapItem*)(dataAddress - sizeof(HeapItem));
	TypeItem elementTypeDesc = typeTable->items[array->typeDesc.innerType];//获取元素类型
	for (auto index = 0; index < array->sol.length; index++)//遍历数组每一项
	{
		if (elementTypeDesc.desc == typeItemDesc::PlaintObj)//数组元素是class
		{
			auto classDesc = classTable->items[elementTypeDesc.innerType];
			if (classDesc->isVALUE)//元素是值类型
			{
				if (elementTypeDesc.innerType == classTable->system_object)//是object类型
				{
					u64 obj = *((u64*)(dataAddress + classDesc->size * index));
					if (obj != 0)
					{
						if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
						{
							auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
							GCClassFieldAnalyze(GCRoots, obj, realType);
						}
					}
				}
				else
				{
					GCClassFieldAnalyze(GCRoots, dataAddress + classDesc->size * index, elementTypeDesc.innerType);
				}
			}
			else//元素是引用类型
			{
				u64 obj = *((u64*)(dataAddress + 8 * index));
				if (obj != 0)
				{
					GCClassFieldAnalyze(GCRoots, obj, elementTypeDesc.innerType);
				}
			}
		}
		else if (elementTypeDesc.desc == typeItemDesc::Array)//数组元素是还是数组
		{
			u64 arr = *((u64*)(dataAddress + 8 * index));
			if (arr != 0)
			{
				if (mark(GCRoots, (HeapItem*)(arr - sizeof(HeapItem))))
				{
					GCArrayAnalyze(GCRoots, arr);
				}
			}
		}
		else//元素是函数类型
		{
			u64 obj = *((u64*)(dataAddress + 8 * index));
			if (obj != 0)
			{
				if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
				{
					auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
					GCClassFieldAnalyze(GCRoots, obj, realType);
				}
			}
		}
	}
}
//使用广度优先搜索标记对象
void VM::GCRootsSearch(VM& vm, std::list<HeapItem*>& GCRoots)
{
	u64 bp = vm.varStack.getBP();
	for (
		auto it = vm.frameStack.rbegin();
		it != vm.frameStack.rend();
		it++)//逆序遍历
	{
		//把变量栈中所有指针放入GCRoot
		FrameItem frameItem = *it;
		auto& frame = vm.stackFrameTable->items[frameItem.frameIndex];
		auto varAddress = frame->baseOffset;
		for (auto i = 0; ; i++)
		{
			if (varAddress >= frameItem.frameSP) {
				//达到目前栈帧已经分配的所有变量位置
				break;
			}
			//如果是引用类型，则size等于8
			auto typeDesc = vm.typeTable->items[frame->items[i].type];
			if (typeDesc.desc == typeItemDesc::PlaintObj)
			{
				if (vm.classTable->items[typeDesc.innerType]->isVALUE)//是值类型
				{
					if (frame->items[i].type == vm.typeTable->system_object)//如果是object，则把他当作引用对待
					{
						u64 obj = *((u64*)((u64)vm.varStack.getBufferAddress() + bp + varAddress));
						if (obj != 0)//不是null
						{
							if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
							{
								auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
								GCClassFieldAnalyze(GCRoots, obj, realType);
							}
						}
					}
					else
					{
						GCClassFieldAnalyze(GCRoots, (u64)vm.varStack.getBufferAddress() + bp + varAddress, typeDesc.innerType);
					}
					varAddress += vm.classTable->items[typeDesc.innerType]->size;//偏移增加
				}
				else
				{
					u64 obj = *((u64*)((u64)vm.varStack.getBufferAddress() + bp + varAddress));
					if (obj != 0)//不是null
					{
						if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
						{
							GCClassFieldAnalyze(GCRoots, obj, typeDesc.innerType);
						}
					}
					varAddress += sizeof(u64);//偏移增加
				}
			}
			else if (typeDesc.desc == typeItemDesc::Array)
			{
				u64 arr = *((u64*)((u64)vm.varStack.getBufferAddress() + bp + varAddress));
				if (arr != 0) //数组不是null
				{
					if (mark(GCRoots, (HeapItem*)(arr - sizeof(HeapItem))))
					{
						GCArrayAnalyze(GCRoots, arr);
					}
				}
				varAddress += sizeof(u64);//偏移增加
			}
			else//元素是函数类型
			{
				u64 obj = *((u64*)((u64)vm.varStack.getBufferAddress() + bp + varAddress));
				if (obj != 0)//不是null
				{
					if (mark(GCRoots, (HeapItem*)(obj - sizeof(HeapItem))))
					{
						auto realType = ((HeapItem*)(obj - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
						GCClassFieldAnalyze(GCRoots, obj, realType);
					}
				}
				varAddress += sizeof(u64);//偏移增加
			}
		}
		bp = frameItem.lastBP;
	}
}
bool VM::mark(std::list<HeapItem*>& GCRoots, HeapItem* pointer)
{
	if (pointer->gcMark != gcCounter)
	{
		pointer->gcMark = gcCounter;
		GCRoots.push_back(pointer);
		return true;
	}
	else
	{
		return false;
	}
}

void VM::setSafePoint()
{
	isSafePoint = true;
	if (!waitGC.try_lock())//如果GC线程已经在等待GC了，则等待GC完毕再返回
	{
		waitGC.lock();//等待
	}
	waitGC.unlock();
	isSafePoint = false;
}

void VM::gc()
{
	int i = 0;
	for (;;) {
		std::cout << i++ << std::endl;
		waitGC.lock();
		//需要注意的是，在C++11之后std::list的size才是O(1)，如果用C++98编译，还是自己实现list比较好
		if (heap.size() >= GCcondition)//如果堆的对象数量小于GCcondition，且不是强制GC，则不进入GC
		{
			for (auto it = VMs.begin(); it != VMs.end(); it++)//等待所有的线程都进入safePoint
			{
				for (; !(*it)->isSafePoint;)
				{
					std::this_thread::yield();
				}
			}


			for (auto it1 = VMs.begin(); it1 != VMs.end(); it1++) {
				VM& vm = *(*it1);
				for (
					auto it = vm.frameStack.rbegin();
					it != vm.frameStack.rend();
					it++)//逆序遍历
				{
					//把变量栈中所有指针放入GCRoot
					FrameItem frameItem = *it;
				}
			}







			std::list<HeapItem*> GCRoots;
			/*下面的代码先标记program，然后对当前VM的变量栈进行分析*/
			gcCounter++;
			if (program != 0)
			{
				if (mark(GCRoots, (HeapItem*)(program - sizeof(HeapItem))))
				{
					auto realType = ((HeapItem*)(program - sizeof(HeapItem)))->typeDesc.innerType;//元素的真实类型
					GCClassFieldAnalyze(GCRoots, program, realType);
				}
			}
			for (auto it = VMs.begin(); it != VMs.end(); it++) {
				GCRootsSearch(**it, GCRoots);
			}
			sweep();
		}
		waitGC.unlock();
		//std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
VM::~VM()
{
	VMs.erase(this);
}
