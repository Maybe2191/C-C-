#pragma once
#include"Common.h"

class ThreadCache
{
public:
	void* Allocte(size_t size);
	void Deallocte(void* ptr, size_t size);
	void* FetchFromCentralCache(size_t size);

	void ListTooLong(FreeList& freelist, size_t num, size_t size);
private:
	FreeList _freelists[BUF_SIZE];
};

_declspec (thread) static ThreadCache* tlsThreadCache = nullptr;