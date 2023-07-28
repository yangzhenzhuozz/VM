#ifndef _HEAP
#define _HEAP
#include "./typeTable.hpp"
struct HeapItem
{
    union SizeOrLength {
        u64 size;//对于plainObj是size
        u64 length;//对于数组是length
    } sol;//对于函数没有意义
    TypeItem typeDesc;
    u64 realTypeName;
    u64 gcMark;//gcMark标记
    bool isNativeResouce = false;//是否native资源
    u64 NativeResourceFreeCallBack;//本地资源类型释放回调
    u64 wrapType;//用于函数类型,包裹类在typeTable中的类型
    u64 text;//用于函数类型
    char data[0];//0长数组sizeof不占用空间(代码中用到了这个特性),对于函数对象，这个obj的内容是包裹类
};
#endif // !_HEAP
/*
* 函数包裹类的工作原理，我也怕忘了
*       (函数对象)
* ┌────────┐
* │HeapItem_header │(里面有个text属性，是函数的代码地址)
* ├────────┤
* │   (8字节指针)  │  -->  ┌────────┐
* └────────┘       │HeapItem_header │
*                            ├────────┤
*                            │      @this     │(如果函数是class内部的，则这个指针指向该class实例对象)
*                            ├────────┤
*                            │  capture_obj_1 │ 
*                            ├────────┤
*                            │  capture_obj_2 │
*                            ├────────┤
*                            │       .....    │
*                            └────────┘
* 
* 这样函数对象就能随意的传递或者返回，永远都能拿到要访问的数据
* 
* 扩展函数工作原理:
* 
* extension function toString(this int value):string{
*           xxxxxx
* }
* int a;
* a.toString();
* 
* 会被替换成如下代码
* 
* int a;
* function(){
*  int tmp=a;
*  return @int_toString(a);//这里就把tmp捕获了,对于值类型则是复制一份
* }
* 
* 如果扩展了一个值类型，在扩展函数中对这个值类型修改的话，实际改的是tmp的值
* 曾经伪装闭包，使扩展函数修改值的时候能真实的改到原来的a，但是这样就会导致无法GC，因为变量a的地址没有HeapItem这一项,capture_obj_1指向了数据真实地址,GC无法分析这个地址是否还可用
* 
*/
