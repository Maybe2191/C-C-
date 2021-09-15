#pragma once
#include <iostream>
#include <assert.h>
#include <unordered_map>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif // _WIN32
using std::cout;
using std::endl;
const size_t MAX_SIZE = 64 * 1024;
const size_t BUF_SIZE = MAX_SIZE / 8;
const size_t MAX_PAGESIZE = 129;
const size_t PAGE_SHIFT = 12;//Calculate page number from address 

inline void*& NextObj(void* ptr)
{
	return *((void**)ptr);
}
class FreeList
{
public:
	void Push(void* ptr)
	{
		NextObj(ptr) = _freelist;
		_freelist = ptr;
		++_num;
	}
	void PushRange(void* begin, void* end,size_t num)
	{
		NextObj(end) = _freelist;
		_freelist = begin;
		_num += num;
	}
	void* Pop()
	{
		void* start = _freelist;
		_freelist = NextObj(_freelist);
		--_num;
		return start;
	}
	size_t PopRange(void*& start,void*& end,size_t num)
	{
		size_t actual_num = 0;
		void* cur = _freelist, *prev = _freelist;
		for (; actual_num < num && cur; ++actual_num)
		{
			prev = cur;
			cur = NextObj(cur);
		}
		start = _freelist;
		end = prev;
		_freelist = cur;
		_num -= actual_num;
		return actual_num;
	}
	bool Empty()
	{
		return _freelist == nullptr;
	}
	size_t Num()
	{
		return _num;
	}
	void Clear()
	{
		_num = 0;
		_freelist = nullptr;
	}
private:
	void* _freelist = nullptr;
	size_t _num = 0;//单链表计算size效率太挫了，不如维护一个size变量
};

class SizeClass
{
public:
	static size_t NumMoveSize(size_t size)
	{
		assert(size);
		int num = static_cast<int>(MAX_SIZE / size);
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}
	static size_t _ListIndex(size_t size, size_t align_shift)
	{
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	static size_t ListIndex(size_t size)
	{
		assert(size <= MAX_SIZE);
		static int group_array[3] = { 16, 56, 56 };
		if (size <= 128)
			return _ListIndex(size, 3);
		else if (size <= 1024)
			return _ListIndex(size - 128, 4) + group_array[0];
		else if (size <= 8192)
			return _ListIndex(size - 1024, 7) + group_array[0] + group_array[1];
		else
			return _ListIndex(size - 8192, 10) + group_array[0] + group_array[1] + group_array[2];

	}
	static size_t _RoundUp(size_t size,size_t alignment)
	{
		return (size + alignment - 1) & (~(alignment - 1));
	}
	static inline size_t RoundUp(size_t size)
	{
		assert(size <= MAX_SIZE);
		if (size <= 128)
			return _RoundUp(size, 8);
		else if (size <= 1024)
			return _RoundUp(size, 16);
		else if (size <= 8192)
			return _RoundUp(size, 128);
		else if (size <= 65536)
			return _RoundUp(size, 1024);
		return -1;
	}
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= 12;
		return npage == 0 ? 1 : npage;
	}
};
//for windows
#ifdef _WIN32
typedef unsigned int PAGE_ID;
#else
typedef unsigned long long PAGE_ID;
#endif
struct Span
{
	PAGE_ID _pageid = 0;
	int _usecount = 0;

	FreeList _freelist;
	size_t _pagesize = 0;

	size_t _objsize = 0;
	Span* _prev = nullptr;
	Span* _next = nullptr;
};

class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	void PushFront(Span* newspan)
	{
		Insert(_head->_next,newspan);
	}
	void PopFront()
	{
		Erase(_head->_next);
	}
	void PushBack(Span* newspan)
	{
		Insert(_head, newspan);
	}
	void PopBack()
	{
		Erase(_head->_prev);
	}
	void Insert(Span* pos, Span* newspan)//insert before pos
	{
		Span* prev = pos->_prev;
		//prev   newspan   pos
		pos->_prev = newspan;
		newspan->_next = pos;
		newspan->_prev = prev;
		prev->_next = newspan;
	}
	void Erase(Span* pos)
	{
		assert(pos != _head);
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}
	bool Empty()
	{
		return _head->_next == _head;
	}
	void Lock()
	{
		_mtx.lock();
	}
	void Unlock()
	{
		_mtx.unlock();
	}
private:
	Span* _head;
	std::mutex _mtx;
};

inline static void* SystemAlloc(size_t num_page)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, num_page * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//brk/mmap
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr,0,MEM_RELEASE);
#else
	//brk/mmap
#endif
}