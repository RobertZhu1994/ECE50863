//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

#include <sstream> // std::ostringstream

#include <zmq.hpp>

#include "config.h"
#include "msgfmt.h"
#include "mydecoder.h"
#include "log.h"
#include "rxtx.h"

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


int main (int argc, char *argv[])
{

	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

	EE("bound to %s. wait for workers to push ...", FRAME_PULL_ADDR);

	CallBackTimer stat_collector;

	stat_collector.start(1000 /*ms, check interval */, &getStatistics);

	size_t sz;
	while (1) {
		recv_one_frame(receiver, &sz);
		stat.inc_byte_counter((int)sz);
		stat.inc_rec_counter(1);
	}

	/* XXX stop the stat collector */
	stat_collector.stop();

	return 0;
}