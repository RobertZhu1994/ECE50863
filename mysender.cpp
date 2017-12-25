//
// Created by xzl on 12/23/17.
//

#include <zmq.hpp>
extern "C" {
#include <libavutil/mem.h>
}
#include "log.h"

/* free frame data */
static void my_free_av (void *data, void *hint)
{
	xzl_bug_on(!data);
	av_freep(&data);
}


/* @buffer allocated from av_malloc. zmq will free it */
int send_one_frame(uint8_t *buffer, int size, zmq::socket_t & sender)
{
	int ret;

	xzl_bug_on(!buffer);

	/* will free @buffer internally */
	zmq::message_t msg(buffer, size, my_free_av);
	ret = sender.send(msg);
	xzl_bug_on(!ret);

	return 0;
}

