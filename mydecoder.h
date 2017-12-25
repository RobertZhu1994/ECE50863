//
// Created by xzl on 12/23/17.
//

#ifndef VIDEO_STREAMER_DECODER_H
#define VIDEO_STREAMER_DECODER_H

#include <zmq.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}

int send_one_frame(uint8_t *buffer, int size, zmq::socket_t & sender);

int decode_one_file_hw(const char *fname, zmq::socket_t &sender);
int decode_one_file_sw(const char *fname, zmq::socket_t &sender);

#endif //VIDEO_STREAMER_DECODER_H
