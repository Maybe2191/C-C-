#include"PageCache.h"
Span* PageCache::_NewSpan(size_t num_page)
{
	if (!_spanlists[num_page].Empty())
	{
		Span* span = _spanlists[num_page].Begin();
		_spanlists[num_page].PopFront();
		return span;
	}
	for (int i = num_page + 1; i < MAX_PAGESIZE; ++i)
	{//所谓分裂并不是真的内存被分开了，而是将原本由一个span管理的内存由两个span来管理,new Span不等于重新申请内存
		if (!_spanlists[i].Empty())
		{
			Span* span = _spanlists[i].Begin();
			_spanlists[i].PopFront();
			Span* splitspan = new Span;
			splitspan->_pageid = span->_pageid + span->_pagesize - num_page;
			splitspan->_pagesize = num_page;
			for (PAGE_ID i = 0; i < splitspan->_pagesize; ++i)
				_id_span_map[splitspan->_pageid + i] = splitspan;
			span->_pagesize -= num_page;
			_spanlists[span->_pagesize].PushFront(span);
			return splitspan;
		}
	}
	void* ptr = SystemAlloc(MAX_PAGESIZE - 1);
	Span* bigspan = new Span;
	bigspan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;//申请内存，通过地址计算页号
	bigspan->_pagesize = MAX_PAGESIZE - 1;
	for (PAGE_ID i = 0; i < bigspan->_pagesize; ++i)
		_id_span_map[bigspan->_pageid + i] = bigspan;
	_spanlists[bigspan->_pagesize].PushFront(bigspan);
	return _NewSpan(num_page);
}
Span* PageCache::NewSpan(size_t num_page)
{
	_mtx.lock();
	Span* span = _NewSpan(num_page);
	_mtx.unlock();
	return span;
}
Span* PageCache::GetIdToSpan(PAGE_ID id)
{
	//return _id_span_map[id];错误写法，万一id没有就插入了，本意是用id查找span*
	auto it = _id_span_map.find(id);
	if (it != _id_span_map.end())
		return it->second;
	return nullptr;
}
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	while (1)
	{
		PAGE_ID previd = span->_pageid - 1;
		auto it = _id_span_map.find(previd);
		if (it == _id_span_map.end())
			break;
		Span* prevspan = it->second;
		if (prevspan->_usecount)
			break;
		//页的合并包括更改span的参数，以及更改映射，最后free一个span
		if (span->_pagesize + prevspan->_pagesize >= MAX_PAGESIZE)
			break;
		span->_pageid = prevspan->_pageid;
		span->_pagesize += prevspan->_pagesize;
		for (PAGE_ID i = 0; i < prevspan->_pagesize; ++i)
			_id_span_map[prevspan->_pageid + i] = span;
		_spanlists[prevspan->_pagesize].Erase(prevspan);//这一点别忘了，从spanlist中删除
		delete prevspan;
	}
	while (1)
	{
		PAGE_ID nextid = span->_pageid + span->_pagesize;
		auto it = _id_span_map.find(nextid);
		if (it == _id_span_map.end())
			break;
		Span* nextspan = it->second;
		if (nextspan->_usecount)
			break;
		if (span->_pagesize + nextspan->_pagesize >= MAX_PAGESIZE)
			break;
		span->_pagesize += nextspan->_pagesize;
		for (PAGE_ID i = 0; i < nextspan->_pagesize; ++i)
			_id_span_map[nextspan->_pageid + i] = span;
		_spanlists[nextspan->_pagesize].Erase(nextspan);
		delete nextspan;
	}
	_spanlists[span->_pagesize].PushFront(span);
}