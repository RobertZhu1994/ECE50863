//
// Created by xzl on 12/23/17.
//

#ifndef VIDEO_STREAMER_DECODER_H
#define VIDEO_STREAMER_DECODER_H

#include <zmq.hpp>

int send_one_frame(uint8_t *buffer, int size, zmq::socket_t & sender);

#endif //VIDEO_STREAMER_DECODER_H
