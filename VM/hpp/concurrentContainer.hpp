#ifndef _CONCURRENTCONTAINER
#define _CONCURRENTCONTAINER
#include <list>
#include <unordered_set>
#include <mutex>
template<class T>
class list_safe
{
	std::list<T> list;
	std::mutex mutex;
public:
	void push_back(const T& v)
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

template<class T>
class set_safe
{
	std::unordered_set<T> set;
	std::mutex mutex;
public:
	void insert(const T& v)
	{
		mutex.lock();
		set.insert(v);
		mutex.unlock();
	}
	typename std::unordered_set<T>::iterator begin()
	{
		mutex.lock();
		auto it = set.begin();
		mutex.unlock();
		return it;
	}
	typename std::unordered_set<T>::iterator end()
	{
		mutex.lock();
		auto it = set.end();
		mutex.unlock();
		return it;
	}
	void  erase(const T& v)
	{
		mutex.lock();
		set.erase(v);
		mutex.unlock();
	}
};
#endif