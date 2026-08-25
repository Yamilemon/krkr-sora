#include "tjsCommHead.h"
#include "StorageCache.h"

// ファイル用アロケータ
extern "C" void *file_malloc(size_t size) { 
	void *r = malloc(size);
	if (!r) {
		TVPClearOldStorageCache(0, false);
		r = malloc(size);
		if (!r) {
			TVPClearOldStorageCache(0, true);
			r = malloc(size);
		}
	}
	return r;
}

extern "C" void *file_realloc(void *p, size_t newSize) {
	void *r = realloc(p, newSize); 
	if (!r) {
		TVPClearOldStorageCache(0, false);
		r = realloc(p, newSize);
		if (!r) {
			TVPClearOldStorageCache(0, true);
			r = realloc(p, newSize);
		}
	}
	return r;
}

extern "C" void file_free(void *p) { free(p); }
