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

using namespace vs;

int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
//	receiver.connect(CLIENT_PULL_FROM_ADDR);
	receiver.bind(FRAME_PULL_ADDR);

	I("bound to %s. wait for workers to push ...", FRAME_PULL_ADDR);

	while (1) {
		recv_one_frame(receiver);
	}

	return 0;
}