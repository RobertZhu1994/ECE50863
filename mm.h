//
// Created by xzl on 12/25/17.
//

#ifndef VIDEO_STREAMER_MM_H
#define VIDEO_STREAMER_MM_H

#include <atomic>
#include <lmdb.h>


#define USE_MALLOC					1
#define USE_MMAP						2
#define USE_AVMALLOC 				3
#define USE_MMAP_REFCNT			4
#define USE_LMDB_REFCNT			5

/* generic alloc info for various allocation methods */

struct my_alloc_hint {

	int flag; /* allocation method */
	size_t length; /* required for mmap. others can be 0 */

	/* for refcnt'd mmap region */
	uint8_t *base;  /* may != the pointer to be free'd */
	std::atomic<long> refcnt; /* can reach neg. must be long since delta is unsigned */

	/* for lmdb trans */
	MDB_txn *txn;
	MDB_cursor *cursor;

	my_alloc_hint(int flag) : flag(flag), refcnt(0) { }

	my_alloc_hint(int flag, size_t length) : flag(flag), length(length), base(nullptr), refcnt(0) {}

	my_alloc_hint(int flag, size_t length, uint8_t *base, int refcnt) : flag(flag), length(length), base(base),
																																			refcnt(refcnt) {}

};

/* if hint == null, do nothing. data to be free'd later */
void my_free(void *data, void *hint);

void map_file(const char *fname, uint8_t **p, size_t *sz);

#endif //VIDEO_STREAMER_MM_H
