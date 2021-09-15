#pragma once
#include"Common.h"

class CentralCache
{
public:
	size_t FetchRangeObj(void*& start, void*& end,size_t num,size_t size);
	void ReleaseListToSpans(void* start, size_t size);
	Span* GetOneSpan(size_t size);
	static CentralCache& GetInstance()
	{
		static CentralCache cencache;
		return cencache;
	}
private:
	CentralCache()
	{}
	CentralCache(CentralCache& cent) = delete;
	SpanList _spanlists[BUF_SIZE];
};
