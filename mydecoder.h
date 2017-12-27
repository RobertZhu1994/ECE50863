//
// Created by xzl on 12/23/17.
//

#ifndef VIDEO_STREAMER_DECODER_H
#define VIDEO_STREAMER_DECODER_H

#include <zmq.hpp>
#include "msgfmt.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}

int send_one_frame(uint8_t *buffer, int size, zmq::socket_t &sender,
									 vstreamer::frame_desc const &desc);

int decode_one_file_hw(const char *fname, zmq::socket_t &sender,
											 vstreamer::chunk_desc const &desc);

int decode_one_file_sw(const char *fname, zmq::socket_t &sender,
											 vstreamer::chunk_desc const &desc);

int send_one_fb(vstreamer::feedback const & fb, zmq::socket_t &sender);

bool recv_one_fb(zmq::socket_t &s, vstreamer::feedback * fb, bool blocking);

#endif //VIDEO_STREAMER_DECODER_H
