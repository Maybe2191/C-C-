#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::Allocte(size_t size)
{
	size_t index = SizeClass::ListIndex(size);
	if (!_freelists[index].Empty())
		return _freelists[index].Pop();
	else
	{
		return FetchFromCentralCache(SizeClass::RoundUp(size));
	}
}
void ThreadCache::Deallocte(void* ptr, size_t size)
{
	size_t index = SizeClass::ListIndex(size);
	FreeList& freelist = _freelists[index];
	freelist.Push(ptr);
	size_t num = SizeClass::NumMoveSize(size);
	if (freelist.Num() >= num)//size is larger than 2M or The quantity is greater than a certain level
	{
		ListTooLong(freelist,num,size);
	}
}
void ThreadCache::ListTooLong(FreeList& freelist, size_t num,size_t size)
{
	void* start = nullptr, *end = nullptr;
	freelist.PopRange(start, end, num);
	NextObj(end) = nullptr;
	CentralCache::GetInstance().ReleaseListToSpans(start,size);
}

void* ThreadCache::FetchFromCentralCache(size_t size)
{
	size_t num = SizeClass::NumMoveSize(size);
	void* start = nullptr, *end = nullptr;
	size_t actual_num = CentralCache::GetInstance().FetchRangeObj(start, end, num, size);
	if (actual_num == 1)
		return start;
	else
	{
		size_t index = SizeClass::ListIndex(size);
		_freelists[index].PushRange(NextObj(start), end, actual_num - 1);
		return start;
	}
}
