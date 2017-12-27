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

using namespace std;
using namespace vstreamer;

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

/* create mmap for the whole given file
 * @p: to be unmapped by caller */
void map_file(const char *fname, char **p, size_t *sz)
{
	/* get file sz */
	struct stat finfo;
	int fd = open(fname, O_RDONLY);
	xzl_bug_on(fd < 0);
	int ret = fstat(fd, &finfo);
	xzl_bug_on(ret != 0);

	char *buff = (char *)mmap(NULL, finfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	xzl_bug_on(buff == MAP_FAILED);

	*p = buff;
	*sz = finfo.st_size;
}

/* send the entier file content as two msgs:
 * a chunk desc; a chunk */
void send_chunk_from_file(const char *fname, zmq::socket_t & sender)
{

	/* send desc */
	struct chunk_desc desc(100, 1000, 1000);
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
	char * buf;
	size_t sz;
	map_file(fname, &buf, &sz);

	auto hint = new my_alloc_hint(USE_MMAP, sz);
	zmq::message_t cmsg(buf, sz, my_free, hint);
	sender.send(cmsg, 0); /* no more msg */

	I("chunk sent");
}

/* send raw frames (from a raw video file) over. */
void send_raw_frames_from_file(const char *fname, zmq::socket_t &s,
													 int height, int width, int yuv_mode,
													 chunk_desc const & cdesc)
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
	char * buf;
	size_t sz, offset;
	map_file(fname, &buf, &sz);

	for (offset = 0; offset < sz; offset += frame_w) {
		send_one_frame()

		if (offset + frame_w >= sz) { /* last frame. to free the whole buffer */


		}
	}

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

	I("bound to %s (fb %s). wait for workers to pull ...",
		CHUNK_PUSH_ADDR, FB_PULL_ADDR);

	printf ("Press Enter when the workers are ready: ");
	getchar ();
	printf ("Sending tasks to workers…\n");

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
	return 0;
}
