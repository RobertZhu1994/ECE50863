//
// Created by xzl on 12/24/17.
//
// tester: push a video clip to the server

// file xfer over zmq
// cf:
// http://zguide.zeromq.org/php:chapter7#toc22
//
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <sstream> // std::ostringstream

#include <zmq.hpp>

#include "mm.h"
#include "config.h"
#include "msgfmt.h"
#include "mydecoder.h"
#include "log.h"
#include "rxtx.h"

using namespace std;
using namespace vs;

void send_chunks(const char *fname, int k)
{
	/* send the file k times */
	for (int i = 0; i < k; i++) {

	}
}

/* load the given file contents to mem
 * @p: allocated by this func, to be free'd by caller */
void load_file(const char *fname, char **p, size_t *sz)
{
	/* get file sz */
	struct stat finfo;
	int fd = open(fname, O_RDONLY);
	xzl_bug_on(fd < 0);
	int ret = fstat(fd, &finfo);
	xzl_bug_on(ret != 0);

	char *buff = (char *)malloc(finfo.st_size);
	xzl_bug_on(!buff);

	auto s = pread(fd, buff, finfo.st_size, 0);
	xzl_bug_on(s != finfo.st_size);

	*p = buff;
	*sz = s;
}

/* send the entier file content as two msgs:
 * a chunk desc; a chunk */
void send_chunk_from_file(const char *fname, zmq::socket_t & sender)
{

	/* send desc */
	struct data_desc desc(100, 1000, 1000);
	ostringstream oss;
	boost::archive::text_oarchive oa(oss);

	oa << desc;
	string s = oss.str();

	/* desc msg. explicit copy content over */
//		zmq::message_t dmsg(s.size());
//		memcpy(dmsg.data(), s.c_str(), s.size());
	zmq::message_t dmsg(s.begin(), s.end());
	sender.send(dmsg, ZMQ_SNDMORE); /* multipart msg */

	I("desc sent");

	/* send chunk */
	uint8_t * buf;
	size_t sz;
	map_file(fname, &buf, &sz);

	auto hint = new my_alloc_hint(USE_MMAP, sz);
	zmq::message_t cmsg(buf, sz, my_free, hint);
	sender.send(cmsg, 0); /* no more msg */

	I("chunk sent");
}

/* given key range, load all the chunks from the db, and send them out as
 * msgs (desc + chunk for each) */
void test_send_chunks_from_db(const char *dbpath, zmq::socket_t & sender)
{
	int rc;
	MDB_env *env;
	MDB_dbi dbi;
	MDB_txn *txn;

	rc = mdb_env_create(&env);
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_mapsize(env, 1UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
	xzl_bug_on(rc != 0);

	rc = mdb_env_open(env, dbpath, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi);
	xzl_bug_on(rc != 0);

	mdb_txn_commit(txn); /* done open the db */

	send_chunks_from_db(env, dbi, 0, 1000 * 1000 * 1000, sender);

	/* -- wait for all outstanding? -- */
	sleep (10);

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);
}


/* send raw frames (from a raw video file) over. */
void send_raw_frames_from_file(const char *fname, zmq::socket_t &s,
													 int height, int width, int yuv_mode,
													 data_desc const & cdesc)
{
	size_t frame_sz = (size_t) width * (size_t) height;
	size_t frame_w;

	if (yuv_mode == 420)
		frame_w = ((frame_sz * 3) / 2);
	else if (yuv_mode == 422)
		frame_w = (frame_sz * 2);
	else if (yuv_mode == 444)
		frame_w = (frame_sz * 3);
	else
		xzl_bug("unsupported yuv");

	/* map file */
	uint8_t * buf = nullptr;
	size_t sz;
	map_file(fname, &buf, &sz);
	xzl_bug_on(!buf);
	auto n_frames = sz / frame_w;  /* # of frames we have */
	xzl_bug_on(n_frames == 0);

	/* all frames come from the same mmap'd file. they share the same @hint */
	auto h = new my_alloc_hint(USE_MMAP_REFCNT, sz, buf, n_frames); /* will be free'd when refcnt drops to 0 */
	for (auto i = 0u; i < n_frames; i++) {
		data_desc fdesc(cdesc.id, i /* frame id */);  /* XXX improve this */
		send_one_frame_mmap(buf + i * frame_w, frame_w, s, fdesc, h);
	}

	I("total %lu raw frames from %s", n_frames, fname);
}

void test_send_raw_frames_from_file(const char *fname, zmq::socket_t &s_frame)
{
	data_desc cdesc(0, 1000, 100); /* random numbers */
	send_raw_frames_from_file(fname, s_frame, 320, 240, 420, cdesc);
}

/* argv[1] -- the video file name */
int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t sender(context, ZMQ_PUSH);
//	sender.connect(CLIENT_PUSH_TO_ADDR);
	sender.bind(CHUNK_PUSH_ADDR);

	zmq::socket_t fb_recv(context, ZMQ_PULL);
	fb_recv.bind(FB_PULL_ADDR);

	zmq::socket_t s_frame(context, ZMQ_PUSH);
	s_frame.connect(WORKER_PUSH_TO_ADDR);  /* push raw frames */

	I("bound to %s (fb %s). wait for workers to pull ...",
		CHUNK_PUSH_ADDR, FB_PULL_ADDR);

	printf ("Press Enter when the workers are ready: ");
	getchar ();
	printf ("Sending tasks to workers…\n");

#if 0
	for (int i = 0; i < 20; i++) {

		I("to send chunk %d/20...", i);
		send_chunk_from_file(argv[1], sender);

		/* check fb */
		I("to recv fb");
		feedback fb;
		bool ret = recv_one_fb(fb_recv, &fb, true /* blocking */);
		if (ret)
			I("got one fb. id %lu", fb.fid);
		else
			I("failed to get fb");
	}
#endif

//	test_send_raw_frames_from_file(argv[1], s_frame);

	test_send_chunks_from_db(DB_PATH, sender);

	return 0;
}
