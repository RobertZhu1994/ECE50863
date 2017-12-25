//
// Created by xzl on 12/25/17.
//

#include <sys/mman.h>
extern "C" {
#include <libavutil/mem.h>
}
#include "log.h"
#include "mm.h"

void my_free(void *data, void *hint)
{
	xzl_bug_on(!data || !hint);

	struct my_alloc_hint *h = (my_alloc_hint *)hint;

	switch (h->flag) {
		case USE_MALLOC:
			free(data);
			break;
		case USE_MMAP: {
			auto ret = munmap(data, h->length);
			xzl_bug_on(ret != 0);
			break;
		}
		case USE_AVMALLOC:
			av_freep(&data);
			break;
		default:
			xzl_bug("unsupported?");
	}

	delete h; /* free hint */
}
