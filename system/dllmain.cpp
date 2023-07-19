#include <stdio.h>
#include <cstring>
#include "../VM/bridge/bridge.hpp"

extern "C" __declspec(dllimport) void system_console_NativePrintBytesString(tpointer dataAdd);

void system_console_NativePrintBytesString(tpointer dataAdd)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 1];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\0';
	printf("%s", buffer);
}