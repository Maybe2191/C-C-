#include"CentralCache.h"
#include"PageCache.h"
Span* CentralCache::GetOneSpan(size_t size)
{
	size_t index = SizeClass::ListIndex(size);
	SpanList& spanlist = _spanlists[index];
	Span* it = spanlist.Begin();
	while (it != spanlist.End())
	{
		if (!it->_freelist.Empty())
			return it;
		else
			it = it->_next;
	}
	size_t num_page = SizeClass::NumMovePage(size);
	Span* span = PageCache::GetInstance().NewSpan(num_page);//从PageCache获取
	char* begin = (char*)(span->_pageid << 12);
	char* end = begin + (span->_pagesize << 12);
	while (begin < end)
	{
		char* obj = begin;
		span->_freelist.Push(obj);
		begin += size;
	}
	span->_objsize = size;
	spanlist.PushFront(span);
	return span;
}
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t size)
{
	size_t index = SizeClass::ListIndex(size);
	SpanList& spanlist = _spanlists[index];
	spanlist.Lock();
	Span* span = GetOneSpan(size);
	FreeList& freelist = span->_freelist;
	size_t actual_num = freelist.PopRange(start, end, num);
	span->_usecount += actual_num;
	spanlist.Unlock();
	return actual_num;
}
void CentralCache::ReleaseListToSpans(void* start,size_t size)
{
	size_t index = SizeClass::ListIndex(size);
	SpanList& spanlist = _spanlists[index];
	spanlist.Lock();
	while (start)
	{
		void* next = NextObj(start);
		PAGE_ID id = (PAGE_ID)start >> PAGE_SHIFT;
		Span* span = PageCache::GetInstance().GetIdToSpan(id);
		span->_freelist.Push(start);
		span->_usecount--;
		if (!span->_usecount)
		{
			size_t index = SizeClass::ListIndex(span->_objsize);
			_spanlists[index].Erase(span);
			span->_freelist.Clear();//如果不清空下一次切块连接的时候本来头插第一块的next应该是nullptr，结果是链到尾巴上去了。
			PageCache::GetInstance().ReleaseSpanToPageCache(span);
		}
		start = next;
	}
	spanlist.Unlock();

}