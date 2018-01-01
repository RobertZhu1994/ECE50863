//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

#ifdef DEBUG
#define GLIBCXX_DEBUG 1 // debugging
#endif

#include <sstream> // std::ostringstream

#include <zmq.hpp>

#include "config.h"
#include "vs-types.h"
#include "mydecoder.h"
#include "log.h"
#include "rxtx.h"
#include "rx-manager.h"

#include "StatCollector.h"
#include "CallBackTimer.h"

using namespace vs;

static StatCollector stat;

static void getStatistics(void) {
	Statstics s;
	if (!stat.ReportStatistics(&s))
		return;

	if (s.rec_persec < 1) /* nothing? */
		return;

	printf("%.2f MB/sec \t %.0f fps \n", s.byte_persec/1024/1024, s.rec_persec);
};

/* pull the source to get info for all streams */
void getAllStreamInfo() {



}

int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

	EE("bound to %s. wait for workers to push ...", FRAME_PULL_ADDR);

	CallBackTimer stat_collector;
	RxManager mg;

	stat_collector.start(200 /*ms, check interval */, &getStatistics);

//	size_t sz = 1;
	while (1) {
#if 0
		unsigned seq = recv_one_frame(receiver, &sz);
		if (sz == 0) {
			EE("got end frame. final seq = %u", seq);
		}
#endif

		data_desc desc;
		auto msg_ptr = recv_one_frame(receiver, &desc);
		if (!msg_ptr) {
			mg.DepositADesc(desc);
			I("got desc: %s", desc.to_string().c_str());
			if (desc.type == TYPE_CHUNK_EOF)
				break;
		} else {
			mg.DepositAFrame(desc, msg_ptr);
			stat.inc_byte_counter((int) msg_ptr->size());
			stat.inc_rec_counter(1);
		}
	}

	/* consumption phase */
	Frame f;
	int rc;
	seq_t wm_c, wm_f;
	while (true) {

		mg.GetWatermarks(&wm_c, &wm_f);
		I("watermarks:  chunk %u frame %u", wm_c, wm_f);

		rc = mg.RetrieveAFrame(&f);
		if (rc == VS_ERR_EOF_CHUNKS)
			break;
		xzl_bug_on(rc != 0);
	}

	/* XXX stop the stat collector */
	stat_collector.stop();

	return 0;
}