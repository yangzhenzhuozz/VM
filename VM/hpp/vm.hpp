#ifndef _VM
#define _VM
#include "./environment.hpp"
#include "./ir.hpp"
#include "./stack.hpp"
#include "./stringPool.hpp"
#include "./classTable.hpp"
#include "./stackFrameTable.hpp"
#include "./symbolTable.hpp"
#include "./typeTable.hpp"
#include "./vm.hpp"
#include "./heap.hpp"
#include "./nativeTable.hpp"
#include <stack>
#include <list>
#include <set>
#include <atomic>
#include <mutex>
#include <windows.h>
struct Catch_item
{
	u64 irAddress = 0;
	u64 type = 0;
};
class Catch_point
{
public:
	std::list<Catch_item> type_list;//能捕获的异常类型表
	u64 varBP = 0;
	u64 varSP = 0;
	u64 frameLevel = 0;
	u64 callStackSP = 0;//函数调用栈
};
struct FrameItem
{
	u64 frameSP;//当前栈帧的SP指针(不是varStack的SP)，用于记录已经分配了多少变量(以varStack的BP作为基地址)
	u64 lastBP;//上一帧的BP
	u64 frameIndex;//在frameTable的下标
	u64 isTryBlock;//是否为tryFrame
};
class VM
{
public:
	static i32 gcCounter;//允许溢出，每次执行gc的时候，计数器+1
	static std::list<HeapItem*> heap;//因为要删除中间的对象，所以用list
	static u64 program;//全局的parogram对象
	static int GCcondition;//触发GC的对象数量
	static std::set<VM*> VMs;//已经创建的VM列表
	static std::mutex waitGC;//用于保证同一时刻只有一个线程在GC

	static StringPool* stringPool;
	static ClassTable* classTable;
	static StackFrameTable* stackFrameTable;
	static SymbolTable* symbolTable;
	static TypeTable* typeTable;
	static IRs* irs;
	static NativeTable* nativeTable;

	static void gc();
	static void addObjectToGCRoot(std::list<HeapItem*>& GCRoots, HeapItem* pointer);//把不需要GC的对象放入GCRoot中
	static volatile bool gcExit;//用于通知GC线程结束运行

	void stackBalancingCheck();
	void gcCheck();

	void run();

	VM();
	~VM();

private:
	volatile bool isSafePoint = false;
	bool yield = false;

	Stack varStack;
	Stack calculateStack;
	Stack callStack;
	//后面改为list，先把问题定位
	std::vector<FrameItem> frameStack;//因为需要遍历，所以用list完成stack的功能
	Stack unwindHandler;//函数
	Stack unwindNumStack;//当前需要回退的数量

	std::stack<Catch_point> catchStack;

	u64 pc = 0;

	HeapItem* currentThread = nullptr;//当前线程的thread

	bool VMError = false;

	u64 newArray(u64 elementType, u32* param, u64 levelLen, u64 level);
	void _throw(u64 type);
	void _VMThrowError(u64 type, u64 init, u64 constructor);
	void pop_stack_map(u64 level, bool isThrowPopup);
	void _new(u64 type);
	void setSafePoint(bool isExit = false);//设置安全点
	void _NativeCall(u64 index);
	void fork(HeapItem* funObj);

	static void GCRootsSearch(VM& vm, std::list<HeapItem*>& GCRoots);//使用广度优先搜索标记对象,会标记所有能访问到的对象
	static void GCClassFieldAnalyze(std::list<HeapItem*>& GCRoots, u64 dataAddress, u64 classIndex);//分析一个对象，把内部的所有引用类型添加到GCRoots
	static void GCArrayAnalyze(std::list<HeapItem*>& GCRoots, u64 dataAddress);//分析一个数组，把内部的所有引用类型添加到GCRoots
	static bool mark(std::list<HeapItem*>& GCRoots, HeapItem* pointer);//用于标记一个可达对象，将可达对象的gc计数器设置为当前的gcCounter,同时也能标记出循环引用的对象(如果一个对象已经被标记，则返回false，避免在循环引用对象中进行循环标记)
	static void sweep();//清除garbage

};
#endif