//
// Created by xzl on 12/23/17.
//

#include <sstream>
#include <zmq.hpp>
extern "C" {
#include <libavutil/mem.h>
}
#include "config.h"
#include "msgfmt.h"
#include "log.h"
#include "mm.h"

using namespace std;

#if 0
/* free frame data */
static void my_free_av (void *data, void *hint)
{
	xzl_bug_on(!data);
	av_freep(&data);
}
#endif

/* @buffer allocated from av_malloc. zmq has to free it */
int send_one_frame(uint8_t *buffer, int size, zmq::socket_t &sender,
									 frame_desc const & fdesc)
{
	int ret;

	xzl_bug_on(!buffer);

	{
		/* send frame desc */
		ostringstream oss;
		boost::archive::text_oarchive oa(oss);

		oa << fdesc;
		string s = oss.str();

		zmq::message_t dmsg(s.begin(), s.end());
		sender.send(dmsg, ZMQ_SNDMORE); /* multipart msg */

		I("desc sent");

		/* send frame */

		auto hint = new my_alloc_hint(USE_AVMALLOC, size);
		zmq::message_t cmsg(buffer, size, my_free, hint);
		ret = sender.send(cmsg, 0); /* no more msg */
		xzl_bug_on(!ret);

		I("frame sent");
	}

	/* will free @buffer internally */
//	zmq::message_t msg(buffer, size, my_free_av);
//	ret = sender.send(msg);
//	xzl_bug_on(!ret);

	return 0;
}


/* recv a desc msg and a chunk msg.
 *
 * for the chunk msg,
 * return a shared ptr of msg, so that we can access its data() later
 * [ there seems no safe way of moving out its data. ]
 */
shared_ptr<zmq::message_t> recv_one_chunk(zmq::socket_t & s, chunk_desc *desc) {
	zmq::context_t context(1 /* # io threads */);

	{
		/* recv the desc */
		zmq::message_t dmsg;
		s.recv(&dmsg);
		I("got desc msg. msg size =%lu", dmsg.size());

		std::string s((char const *)dmsg.data(), dmsg.size()); /* copy over */
		std::istringstream ss(s);
		boost::archive::text_iarchive ia(ss);

		ia >> *desc;
		I("key %lu length_ms %lu", desc->id, desc->length_ms);
	}

	{
		/* recv the chunk */
		auto cmsg = make_shared<zmq::message_t>();
		xzl_bug_on(!cmsg);
		auto ret = s.recv(cmsg.get());
		xzl_bug_on(!ret); /* EAGAIN? */
		I("got chunk msg. size =%lu", cmsg->size());

		xzl_bug_on_msg(cmsg->more(), "multipart msg should end");

		return cmsg;
	}
}

/* recv a desc and a chunk from socket.
 * @p: [OUT] mem buffer from malloc(). to be free'd by the caller
 */
void recv_one_chunk_to_buf(zmq::socket_t & s, chunk_desc *desc,
char **p, size_t *sz) {

	auto cmsg = recv_one_chunk(s, desc);

	char * buf = (char *)malloc(cmsg->size());
	xzl_bug_on(!buf);

	memcpy(buf, cmsg->data(), cmsg->size());

	*p = buf;
	*sz = cmsg->size();

	/* cmsg will be auto destroyed */
}

/* recv one chunk, and save it as a new file */
void recv_one_chunk_tofile(zmq::socket_t & s, chunk_desc *desc,
const char * fname) {

	xzl_bug_on(!fname);

	char * buf = nullptr;
	size_t sz;
	recv_one_chunk_to_buf(s, desc, &buf, &sz);

	I("going to write to file. sz = %lu", sz);

	/* write the chunk to file */
	FILE *f = fopen(fname, "wb");
	xzl_bug_on_msg(!f, "failed to cr file");
	auto ret = fwrite(buf, 1, sz, f);
	if (ret != sz)
		perror("failed to write");

	xzl_bug_on(ret != sz);
	fclose(f);
	I("written chunk to file %s", fname);

	free(buf);
}

/* XXX */
/* recv one chunk, and append to an opened file (stream) */