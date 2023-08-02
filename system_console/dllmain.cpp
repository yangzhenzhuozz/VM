#include "../VM/bridge/bridge.hpp"
#include <cstring>
#include <iostream>

using namespace VMStaticExport;

extern "C" __declspec(dllimport) void system_console_NativePrintBytesString(tpointer dataAdd, VM * vm);

void system_console_NativePrintBytesString(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 1];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\0';
	std::cout << buffer << std::endl;
	delete[] buffer;
}

extern "C" __declspec(dllimport) int system_console_ReadLineFromConsole(VM * vm);

int system_console_ReadLineFromConsole(VM* vm)
{
	char c;
	c = std::cin.get();
	return c;
}