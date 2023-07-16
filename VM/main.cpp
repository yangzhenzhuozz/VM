#include <iostream>
#include "iostream"
#include "./hpp/stringPool.hpp"
#include "./hpp/classTable.hpp"
#include "./hpp/stackFrameTable.hpp"
#include "./hpp/nativeTable.hpp"
#include "./hpp/symbolTable.hpp"
#include "./hpp/typeTable.hpp"
#include "./hpp/ir.hpp"
#include "./hpp/vm.hpp"
int main(int argc, char** argv)
{
	std::string baseDir = "";
	int GCcondition = 100;
	for (auto i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-GC") == 0)
		{
			i++;
			sscanf(argv[i], "%d", &GCcondition);
		}
		else
		{
			baseDir = argv[i];
			baseDir += '/';//加上目录分隔符
		}
	}
	if (baseDir == "") {
		baseDir = "./";
	}
	StringPool stringPool((baseDir + "stringPool.bin").c_str());
	NativeTable nativeTable((baseDir + "nativeTable.bin").c_str(), stringPool);
	ClassTable classTable((baseDir + "classTable.bin").c_str(), stringPool);
	StackFrameTable stackFrameTable((baseDir + "stackFrameTable.bin").c_str());
	SymbolTable symbolTable((baseDir + "irTable.bin").c_str());
	TypeTable typeTable((baseDir + "typeTable.bin").c_str(), stringPool);
	IRs irs((baseDir + "text.bin").c_str());
	VM vm(stringPool, classTable, stackFrameTable, symbolTable, typeTable, irs, nativeTable, GCcondition);
	try
	{
		std::cout << "GC的时候要考虑多线程，否则其他线程的数据会被GC掉" << std::endl;
		vm.run();
		return 0;
	}
	catch (const char* err)
	{
		if (strcmp(err, "栈不平衡") == 0)
		{
			return 1;//栈不平衡抛出的异常，当有异常抛出且未被捕获时触发
		}
		else
		{
			//GC错误
			std::cerr << "其他异常" << std::endl;
			return 1;
		}
	}
}