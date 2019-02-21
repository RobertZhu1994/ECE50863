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

#include <zmq.h>
#include <zmq.hpp>

#include "mm.h"
#include "config.h"
#include "vs-types.h"
#include "mydecoder.h"
#include "log.h"
#include "measure.h"
#include "rxtx.h"

using namespace std;
using namespace vs;

atomic<int> flag(0);
//MDB_env *env;

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
	struct data_desc desc(TYPE_CHUNK);
	desc.cid.as_uint = 100; /* XXX more */

	ostringstream oss;
	boost::archive::text_oarchive oa(oss);

	oa << desc;
	string s = oss.str();

	/* desc msg. explicit copy content over */
	zmq::message_t dmsg(s.size());
	memcpy(dmsg.data(), s.c_str(), s.size());
	//zmq::message_t dmsg(s.begin(), s.end());
	//zmq::message_t dmsg(s.size());
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

/* load all the chunks in a key range from the db, and send them out as
 * msgs (desc + chunk for each)
 *
 * this can be used for sending either encoded chunks or raw frames.
 *
 */
void test_send_multi_from_db(const char *dbpath, zmq::socket_t & sender, int type, int f_start, int f_end)
//void test_send_multi_from_db(const char *dbpath, zmq::socket_t & sender, int type)
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

	data_desc temp_desc(type);
	temp_desc.cid.stream_id = 1001;
	temp_desc.c_seq = 0;

//	data_desc temp_desc(type);

	//unsigned cnt = send_multi_from_db(env, dbi, 0, UINT64_MAX, sender, temp_desc);
    unsigned cnt = send_multi_from_db(env, dbi, f_start, f_end, sender, temp_desc);
//	unsigned cnt = send_multi_from_db(env, dbi, 0, 1000 * 1000, sender, temp_desc);


	/* -- wait for all outstanding? -- */
	sleep (10);

	EE("total %u loaded & sent", cnt);

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);
}

void test_send_multi_from_db(const char *dbpath, zmq::socket_t & sender, int type)
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

    data_desc temp_desc(type);
    temp_desc.cid.stream_id = 1001;
    temp_desc.c_seq = 0;

    //	data_desc temp_desc(type);

    //unsigned cnt = send_multi_from_db(env, dbi, 0, UINT64_MAX, sender, temp_desc);
    unsigned cnt = send_chunk_from_db(env, dbi, 0, UINT64_MAX, sender, temp_desc);
    //	unsigned cnt = send_multi_from_db(env, dbi, 0, 1000 * 1000, sender, temp_desc);


    /* -- wait for all outstanding? -- */
    sleep (10);

    EE("total %u loaded & sent", cnt);

    mdb_dbi_close(env, dbi);
    mdb_env_close(env);
}

void test_send_one_from_db(const char *dbpath, zmq::socket_t & sender, int type, int f_start, int f_end)
{

	int rc;

	MDB_env *env;
	MDB_dbi dbi;
	MDB_txn *txn;

	rc = mdb_env_create(&env);
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_mapsize(env, 1UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
	xzl_bug_on(rc != 0);

	flag = 1;

	rc = mdb_env_set_maxdbs(env, 4);
    xzl_bug_on(rc != 0);

    mdb_env_set_maxreaders(env, 30);

	rc = mdb_env_open(env, dbpath, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi);
	xzl_bug_on(rc != 0);

	mdb_txn_commit(txn); /* done open the db */

	data_desc temp_desc(type);
	temp_desc.cid.stream_id = 1001;
	temp_desc.c_seq = 1;

//	data_desc temp_desc(type);

	unsigned cnt = send_multi_from_db(env, dbi, f_start, f_end, sender, temp_desc);
    EE("frame %u loaded & sent from db %s", f_start, dbpath);

    //std::atomic_thread_fence(std::memory_order_release);

    asm volatile("" ::: "memory");
	//free(env);

    //mdb_dbi_close(env, dbi);
	//mdb_env_close(env);
}

/* send raw frames (from a raw video file) over. */
void send_raw_frames_from_file(const char *fname, zmq::socket_t &s,
													 int height, int width, int yuv_mode,
													 data_desc const & fdesc_temp)
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
		data_desc fdesc(fdesc_temp);
		fdesc.f_seq= i;
		send_one_frame_mmap(buf + i * frame_w, frame_w, s, fdesc, h);
	}

	I("total %lu raw frames from %s", n_frames, fname);
}

void test_send_raw_frames_from_file(const char *fname, zmq::socket_t &s_frame)
{
	data_desc fdesc(TYPE_RAW_FRAME);
	send_raw_frames_from_file(fname, s_frame, 320, 240, 420, fdesc);
}

static zmq::context_t context (1 /* # io threads */);

#if 0 /* tbd */
static void *serv_stream_info(void *t) {

	zmq::socket_t si_sender(context, ZMQ_REP);
	si_sender.setsockopt(ZMQ_SNDHWM, 1);
	si_sender.bind(STREAMINFO_PUSH_ADDR);

	EE("launched.");

	while (1) {
		zmq::message_t req;
		si_sender.recv(&req);

		send_all_stream_info(si_sender);
	}

	return nullptr;
}
#endif

void testing_send_one_from_db(MDB_env *env, const char *dbpath, zmq::socket_t & sender, int type, int f_start, int f_end)
{

    int rc;

    MDB_dbi dbi;
    MDB_txn *txn;

    rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	I("%s",mdb_strerror(rc));
    if(rc != 0){
		I("rc = %d", rc);
	}
	xzl_bug_on(rc != 0);

    rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi);
    xzl_bug_on(rc != 0);

    rc =  mdb_txn_commit(txn); /* done open the db */
	xzl_bug_on(rc != 0);

    data_desc temp_desc(type);
    temp_desc.cid.stream_id = 1001;
    temp_desc.c_seq = 1;

    unsigned cnt = send_multi_from_db(env, dbi, f_start, f_end, sender, temp_desc);

    EE("%u frames loaded & sent from db %s", cnt, dbpath);
	//mdb_txn_abort(txn);
    mdb_dbi_close(env, dbi);
    //mdb_env_close(env);
}

/* argv[1] -- the video file name */
int main (int argc, char *argv[])
{

	zmq::socket_t sender(context, ZMQ_PUSH);
//	sender.connect(CLIENT_PUSH_TO_ADDR);
	sender.bind(CHUNK_PUSH_ADDR);

	zmq::socket_t fb_recv(context, ZMQ_PULL);
	fb_recv.bind(FB_PULL_ADDR);

	//I("bound to %s (fb %s). wait for workers to pull ...", CHUNK_PUSH_ADDR, FB_PULL_ADDR);

	zmq::socket_t s_frame(context, ZMQ_REP);
	s_frame.bind(REQUEST_PULL_ADDR);  /* push raw frames */

	//I("connect to %s. ready to push raw frames", WORKER_PUSH_TO_ADDR);

    zmq::socket_t frame_range(context, ZMQ_PUSH);
    frame_range.connect(WORKER_PUSH_TO_ADDR);  /* pull query requests */

    //I("connect to %s. ready to get new requests", WORKER_REQUEST);

//	pthread_t si_server;
//	int rc = pthread_create(&si_server, NULL, serv_stream_info, NULL);
//	xzl_bug_on(rc != 0);
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
#if 1
    int rc;
    MDB_env *env[2];

	//MDB_dbi dbi;
    //MDB_txn *txn;
	for(int i = 0; i < 2; i++){
		rc = mdb_env_create(&env[i]);
		xzl_bug_on(rc != 0);

		rc = mdb_env_set_mapsize(env[i], 1UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
		xzl_bug_on(rc != 0);
	}

	rc = mdb_env_open(env[0], DB_RAW_FRAME_PATH_NVME_720, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_env_open(env[1], DB_RAW_FRAME_PATH_NVME_540, 0, 0664);
	xzl_bug_on(rc != 0);

#endif

	request_desc *rd;

    while(1){
        zmq::message_t q_request(sizeof(request_desc));
        zmq::message_t reply(5);

        I("bound to %s. ready to pull new requests", REQUEST_PULL_ADDR);

        s_frame.recv(&q_request);

		rd = (request_desc *)(q_request.data()), q_request.size();

        I("got desc msg. db_seq =%lu. start =%lu frame_num=%lu", rd->db_seq, rd->start_fnum, rd->total_fnum);

        switch (rd->db_seq) {
            case HDD_RAW_180:
                I("connect to %s. ready to push raw frames", WORKER_PUSH_TO_ADDR);
                //	test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                memcpy (reply.data (), "World", 5);
                s_frame.send(reply);

                test_send_multi_from_db(DB_RAW_FRAME_PATH, frame_range, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 2 * rd->total_fnum);
                //test_send_multi_from_db(DB_RAW_FRAME_PATH, s_frame, TYPE_RAW_FRAME);
                break;
            case NVME_RAW_720:
                //EE("flag = ", flag);
                I("connect to %s. ready to push raw frames", REQUEST_PULL_ADDR);
                //test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                //test_send_multi_from_db(DB_RAW_FRAME_PATH_720, frame_range, TYPE_RAW_FRAME, 2 * rd->start_fnum, 2 * rd->start_fnum + 1);
                //test_send_one_from_db(DB_RAW_FRAME_PATH_SSD_720, s_frame, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 3);
                testing_send_one_from_db(env[0], DB_RAW_FRAME_PATH_NVME_720, s_frame, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 2 * rd->total_fnum + 1);
				break;
            case NVME_RAW_540:
                //EE("flag = ", flag);
                I("connect to %s. ready to push raw frames", REQUEST_PULL_ADDR);
                //	test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                //test_send_one_from_db(DB_RAW_FRAME_PATH_SSD_540, s_frame, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 3);
                testing_send_one_from_db(env[1], DB_RAW_FRAME_PATH_NVME_540, s_frame, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 2 * rd->total_fnum + 1);
				break;
            case HDD_RAW_360:
                I("connect to %s. ready to push raw frames", REQUEST_PULL_ADDR);
                //	test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                //test_send_one_from_db(DB_RAW_FRAME_PATH_SSD_360, s_frame, TYPE_RAW_FRAME, 2 * rd->start_fnum + 2, 2 * rd->start_fnum + 3);
                break;
            case NVME_CHUNK_720:
                I("connect to %s. ready to push raw frames", REQUEST_PULL_ADDR);
                //test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                break;
            case NVME_CHUNK_180:
                I("connect to %s. ready to push raw frames", REQUEST_PULL_ADDR);
                test_send_multi_from_db(DB_PATH, sender, TYPE_CHUNK);
                break;
            default:
                break;
        }
    }

	//	rc = pthread_join(si_server, nullptr); /* will never join.... XXX */
	//	xzl_bug_on(rc != 0);

	return 0;
}
