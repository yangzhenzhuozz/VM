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
	for (auto i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-GC") == 0)
		{
			i++;
			std::ignore = sscanf(argv[i], "%d", &VM::GCObjectNum);
		}
		else if (strcmp(argv[i], "-GCWT") == 0)
		{
			i++;
			std::ignore = sscanf(argv[i], "%d", &VM::GCWaitTime);
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
	VM::stringPool = &stringPool;
	VM::classTable = &classTable;
	VM::stackFrameTable = &stackFrameTable;
	VM::symbolTable = &symbolTable;
	VM::typeTable = &typeTable;
	VM::irs = &irs;
	VM::nativeTable = &nativeTable;

	VM vm;

	std::thread gcThread(VM::gc);//启动GC线程
	gcThread.detach();
	vm.run();

	return 0;
}