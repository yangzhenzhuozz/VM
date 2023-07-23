#include "../VM/bridge/bridge.hpp"
#include <cstring>
#include <stdio.h>

using namespace VMStaticExport;

extern "C" __declspec(dllimport) void system_console_NativePrintBytesString(tpointer dataAdd, VM * vm);

void system_console_NativePrintBytesString(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 2];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\n';
	buffer[pointer->sol.length + 1] = '\0';
	printf("%s", buffer);
}