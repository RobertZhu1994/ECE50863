//
// Created by xzl on 12/24/17.
//
// tester: pull frames from a

#include <zmq.hpp>

#include "config.h"
#include "log.h"

int main (int argc, char *argv[])
{
	zmq::context_t context (1 /* # io threads */);

	zmq::socket_t receiver(context, ZMQ_PULL);
	receiver.connect(CLIENT_PULL_FROM_ADDR);

	I("start to rx msgs...");

	while (1) {
		zmq::message_t message;
		receiver.recv(&message);
		I("got a msg. size %lu", message.size());
	}

	return 0;
}