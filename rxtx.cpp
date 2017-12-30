//
// Created by xzl on 12/23/17.
//

#include "mydecoder.h"
#include <lmdb.h>
#include <sstream>
#include <zmq.hpp>
extern "C" {
#include <libavutil/mem.h>
#include "measure.h"
}
#include "config.h"
#include "msgfmt.h"
#include "log.h"
#include "mm.h"
#include "rxtx.h"

using namespace std;
using namespace vs;

#if 0
/* free frame data */
static void my_free_av (void *data, void *hint)
{
	xzl_bug_on(!data);
	av_freep(&data);
}
#endif

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

	k2_measure("chunk recv'd");

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

	k2_measure("file written");
}

/* XXX */
/* recv one chunk, and append to an opened file (stream) */int send_one_fb(feedback const & fb, zmq::socket_t &sender)
{
	/* send frame desc */
	ostringstream oss;
	boost::archive::text_oarchive oa(oss);

	oa << fb;
	string s = oss.str();

	zmq::message_t dmsg(s.begin(), s.end());
	auto sz = dmsg.size();

	auto ret = sender.send(dmsg);
	if (!ret)
		EE("send failure");
	else
		I("send fb. id = %d. msg size %lu", fb.fid, sz);

	return 0;
}

/* true if a feedback is recv'd */
bool recv_one_fb(zmq::socket_t &s, feedback * fb, bool blocking = false)
{
	zmq::message_t dmsg;
	xzl_bug_on(!fb);

	bool ret = s.recv(&dmsg, blocking ? 0 : ZMQ_DONTWAIT);
//	bool ret = s.recv(&dmsg);

	if (ret) {
		I("got fb msg. msg size =%lu", dmsg.size());

		string ss((char const *) dmsg.data(), dmsg.size()); /* copy over */
		istringstream iss(ss);
		boost::archive::text_iarchive ia(iss);

		ia >> *fb;
	}

	return ret;
}

/* no tx is for the db is alive as of now
 *
 * @dbi: must be opened already.
 *
 * @start/end: inclusive/exclusive
 *
 * @return: total chunks sent.
 * */
unsigned send_chunks_from_db(MDB_env* env, MDB_dbi dbi, cid_t start, cid_t end, zmq::socket_t & s)
{
	MDB_txn *txn;
	MDB_val key, data;
	MDB_cursor *cursor;
	MDB_cursor_op op;
	int rc;

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_cursor_open(txn, dbi, &cursor);
	xzl_bug_on(rc != 0);

	my_alloc_hint *hint = new my_alloc_hint(USE_LMDB_REFCNT);
	hint->txn = txn;
	hint->cursor = cursor;

	int cnt = 0;

	/* once we start to send, the refcnt can be dec by the async sender, which means it can go neg. */
	while (1) {
		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
		if (rc == MDB_NOTFOUND)
			break;

		cid_t id = *(cid_t *)key.mv_data;

		if (id >= end)
			break;

		I("got one k/v. key: %lu, sz %zu data sz %zu",
			*(uint64_t *)key.mv_data, key.mv_size,
			data.mv_size);

		chunk_desc desc;
		desc.id = id;
		desc.size = key.mv_size;
		/* XXX more */

		send_one_chunk_from_db((uint8_t *)data.mv_data, data.mv_size, s, desc, hint);
		cnt ++;
	}

	/* bump refcnt one time */
	int before = hint->refcnt.fetch_add(cnt);
	xzl_bug_on(before < - cnt ); /* impossible */

	if (before == - cnt) { /* meaning that refcnt reaches 0... all outstanding chunks are sent */
		W("close the tx");
		mdb_cursor_close(cursor);
		mdb_txn_abort(txn);
	}

	return cnt;
}

/* send a raw frame, which is allocated by avmalloc.
 * the ownership of the frame is moved to zmq, which will free the frame later.
 *
 * @fdesc: frame descriptor, content from which will be copied & serailized into the msg
 * @buffer allocated from av_malloc. zmq has to free it
 *
 * if buffer == nullptr, send a desc msg only.
 */
int send_one_frame(uint8_t *buffer, int size, zmq::socket_t &sender,
									 frame_desc const & fdesc)
{
	int ret;

	if (!buffer) {
		/* send frame desc */
		ostringstream oss;
		boost::archive::text_oarchive oa(oss);

		oa << fdesc;
		string s = oss.str();

		zmq::message_t dmsg(s.begin(), s.end());
		sender.send(dmsg, 0); /* no more */

		return 0;
	}

	{
		/* send frame desc */
		ostringstream oss;
		boost::archive::text_oarchive oa(oss);

		oa << fdesc;
		string s = oss.str();

		zmq::message_t dmsg(s.begin(), s.end());
		sender.send(dmsg, ZMQ_SNDMORE); /* multipart msg */

		VV("desc sent");

		/* send frame */

		my_alloc_hint *hint = new my_alloc_hint(USE_AVMALLOC, size);
		zmq::message_t cmsg(buffer, size, my_free, hint);
		ret = sender.send(cmsg, 0); /* no more msg */
		xzl_bug_on(!ret);

		VV("frame sent");
	}

	/* will free @buffer internally */
//	zmq::message_t msg(buffer, size, my_free_av);
//	ret = sender.send(msg);
//	xzl_bug_on(!ret);

	return 0;
}

/* send a raw frame from a mmap'd buffer.
 * @hint: the info about the mmap'd buffer (inc refcnt) for zmq to perform unmapping
 *
 * @others: see above.
 */
int send_one_frame_mmap(uint8_t *buffer, size_t sz, zmq::socket_t &sender,
												frame_desc const & fdesc, my_alloc_hint * hint)
{
	xzl_bug_on(!buffer || !hint);

	/* send frame desc */
	ostringstream oss;
	boost::archive::text_oarchive oa(oss);

	oa << fdesc;
	string s = oss.str();

	zmq::message_t dmsg(s.begin(), s.end());
	sender.send(dmsg, ZMQ_SNDMORE); /* multipart msg */

	VV("desc sent");

	/* send the frame */

	zmq::message_t cmsg(buffer, sz, my_free, (void *)hint);
	auto ret = sender.send(cmsg, 0); /* no more msg */
	xzl_bug_on(!ret);

	VV("frame sent");

	return 0;
}

/* send one buf (chunk) returned from a live lmdb transaction */
int send_one_chunk_from_db(uint8_t * buffer, size_t sz, zmq::socket_t &sender,
												chunk_desc const & cdesc, my_alloc_hint * hint) {
	xzl_bug_on(!buffer || !hint);

	/* send frame desc */
	ostringstream oss;
	boost::archive::text_oarchive oa(oss);

	oa << cdesc;
	string s = oss.str();

	zmq::message_t dmsg(s.begin(), s.end());
	sender.send(dmsg, ZMQ_SNDMORE); /* multipart msg */

	VV("desc sent");

	/* send the chunk */
	zmq::message_t cmsg(buffer, sz, my_free, (void *) hint);
	auto ret = sender.send(cmsg, 0); /* no more msg */
	xzl_bug_on(!ret);

	VV("frame sent");

	return 0;
}

/* XXX: do something to the frame.
 * return: frame id extracted from the desc.  */
int recv_one_frame(zmq::socket_t & recv) {

	I("start to rx msgs...");

	frame_desc desc;

	{
		/* recv the desc */
		zmq::message_t dmsg;
		recv.recv(&dmsg);
		I("got desc msg. msg size =%lu", dmsg.size());

		std::string s((char const *)dmsg.data(), dmsg.size()); /* copy over */
		std::istringstream ss(s);
		boost::archive::text_iarchive ia(ss);

		ia >> desc;
		I("cid %lu fid %d", desc.cid, desc.fid);
	}

	if (desc.fid != -1) {	/* recv the frame */
		zmq::message_t cmsg;
		recv.recv(&cmsg);
		I("got frame msg. size =%lu", cmsg.size());

		xzl_bug_on_msg(cmsg.more(), "there should be no more");
	}

	return desc.fid;
}