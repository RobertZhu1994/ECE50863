//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

#include <sstream> // std::ostringstream

#include <zmq.hpp>

#include "config.h"
#include "msgfmt.h"
#include "log.h"

using namespace vstreamer;

void recv_one_frame(zmq::socket_t & recv) {

	I("start to rx msgs...");

	{
		/* recv the desc */
		zmq::message_t dmsg;
		recv.recv(&dmsg);
		I("got desc msg. msg size =%lu", dmsg.size());

		std::string s((char const *)dmsg.data(), dmsg.size()); /* copy over */
		frame_desc desc;
		std::istringstream ss(s);
		boost::archive::text_iarchive ia(ss);

		ia >> desc;
		I("cid %lu fid %lu", desc.cid, desc.fid);
	}

	{	/* recv the frame */
		zmq::message_t cmsg;
		recv.recv(&cmsg);
		I("got frame msg. size =%lu", cmsg.size());

		xzl_bug_on_msg(cmsg.more(), "there should be no more");
	}
}

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