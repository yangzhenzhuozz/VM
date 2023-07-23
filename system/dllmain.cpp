#include <stdio.h>
#include <cstring>
#include <mutex>
#include "../VM/bridge/bridge.hpp"

using namespace VMStaticExport;

extern "C" __declspec(dllimport) void system_console_NativePrintBytesString(tlong dataAdd, VM * vm);

void system_console_NativePrintBytesString(tlong dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 2];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\n';
	buffer[pointer->sol.length + 1] = '\0';
	printf("%s", buffer);
}

void freeMutex(tlong p)
{
	delete (std::mutex*)p;
}

extern "C" __declspec(dllimport) tlong main_getMutex(VM * vm);
tlong main_getMutex(VM* vm)
{
	std::mutex* p = new std::mutex;
	auto ret = vm->addNativeResourcePointer((tlong)p, (tlong)freeMutex);
	return (tlong)ret->data;
} 