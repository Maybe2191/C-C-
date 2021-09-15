#pragma once
#include"Common.h"
class PageCache
{
public:
	Span* NewSpan(size_t num_page);
	Span* _NewSpan(size_t num_page);
	void ReleaseSpanToPageCache(Span* span);
	Span* GetIdToSpan(PAGE_ID id);
	static PageCache& GetInstance()
	{
		static PageCache pagecache;
		return pagecache;
	}
private:
	PageCache()
	{}
	PageCache(const PageCache& pc) = delete;
	SpanList _spanlists[MAX_PAGESIZE];
	std::unordered_map < PAGE_ID, Span* > _id_span_map;
	std::mutex _mtx;
};
