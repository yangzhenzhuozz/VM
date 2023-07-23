#include <mutex>
#include "../VM/bridge/bridge.hpp"

using namespace VMStaticExport;


void freeMutex(tlong p)
{
	delete (std::mutex*)p;
}

extern "C" __declspec(dllimport) tlong system_dispatcher_generateMutex(VM * vm);
tlong system_dispatcher_generateMutex(VM* vm)
{
	std::mutex* p = new std::mutex;
	return vm->addNativeResourcePointer((tlong)p, (tlong)freeMutex);
}


extern "C" __declspec(dllimport) void system_dispatcher_mutexLock(tpointer dataAdd, VM * vm);
void system_dispatcher_mutexLock(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	u64 p = *(u64*)(pointer)->data;
	((std::mutex*)p)->lock();
}

extern "C" __declspec(dllimport) void system_dispatcher_mutexUnlock(tpointer dataAdd, VM * vm);
void system_dispatcher_mutexUnlock(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	u64 p = *(u64*)(pointer)->data;
	((std::mutex*)p)->unlock();
}