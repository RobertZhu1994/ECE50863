//
// Created by xzl on 12/25/17.
//

#ifndef VIDEO_STREAMER_MM_H
#define VIDEO_STREAMER_MM_H


#define USE_MALLOC		1
#define USE_MMAP			2
#define USE_AVMALLOC 	3

struct my_alloc_hint {
	int flag; /* allocation method */
	size_t length; /* required for mmap. others can be 0 */

	my_alloc_hint(int flag, size_t length) : flag(flag), length(length) {}
};

void my_free(void *data, void *hint);

#endif //VIDEO_STREAMER_MM_H
