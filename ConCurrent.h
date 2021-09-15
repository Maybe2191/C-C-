#pragma once
#include "ThreadCache.h"
#include "PageCache.h"
static void* ConcurrentMalloc(size_t size)
{

	if (size <= MAX_SIZE)// < 64K
	{
		if (!tlsThreadCache)
			tlsThreadCache = new ThreadCache;
		return tlsThreadCache->Allocte(size);
	}
	else if (size <= ((MAX_PAGESIZE - 1) << PAGE_SHIFT))
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t page_num = (align_size >> PAGE_SHIFT);
		Span* span = PageCache::GetInstance().NewSpan(page_num);
		span->_objsize = align_size;
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		size_t align_size = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t page_num = (align_size >> PAGE_SHIFT);
		return SystemAlloc(page_num);
	}
}
static void ConcurrentFree(void* ptr)
{
	PAGE_ID id = (PAGE_ID)ptr >> PAGE_SHIFT;
	Span* span = PageCache::GetInstance().GetIdToSpan(id);
	if (span == nullptr)
	{
		SystemFree(ptr);
		return;
	}
	size_t size = span->_objsize;
	if (size <= MAX_SIZE)
		tlsThreadCache->Deallocte(ptr,size);
	else if (size <= ((MAX_PAGESIZE - 1) << PAGE_SHIFT))
		PageCache::GetInstance().ReleaseSpanToPageCache(span);
}