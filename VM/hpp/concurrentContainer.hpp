#ifndef _CONCURRENTCONTAINER
#define _CONCURRENTCONTAINER
#include <list>
#include <mutex>
template<class T>
//typedef int T;
class list_safe
{
	std::list<T> list;
	std::mutex mutex;
public:
	void push_back(T& v)
	{
		mutex.lock();
		list.push_back(v);
		mutex.unlock();
	}
	typename std::list<T>::iterator begin()
	{
		mutex.lock();
		auto it = list.begin();
		mutex.unlock();
		return it;
	}
	typename std::list<T>::iterator end()
	{
		mutex.lock();
		auto it = list.end();
		mutex.unlock();
		return it;
	}
	typename std::list<T>::iterator erase(typename std::list<T>::iterator it)
	{
		mutex.lock();
		auto ret = list.erase(it);
		mutex.unlock();
		return ret;
	}
	bool empty()
	{
		bool ret;
		mutex.lock();
		ret = list.empty();
		mutex.unlock();
		return ret;
	}
	size_t size()
	{
		size_t ret;
		mutex.lock();
		ret = list.size();
		mutex.unlock();
		return ret;
	}
};
#endif