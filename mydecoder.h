//
// Created by xzl on 12/23/17.
//

#ifndef VIDEO_STREAMER_DECODER_H
#define VIDEO_STREAMER_DECODER_H

#include <zmq.hpp>
#include <lmdb.h>

#include "msgfmt.h"
#include "mm.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}

void init_decoder(bool use_hw);

/* should go to rxtx.h */
int send_one_frame(uint8_t *buffer, int size, zmq::socket_t &sender,
									 vs::frame_desc const &desc);

int send_one_frame_mmap(uint8_t *buffer, size_t sz, zmq::socket_t &sender,
												vs::frame_desc const & fdesc, my_alloc_hint * hint);

int decode_one_file_hw(const char *fname, zmq::socket_t &sender,
											 vs::chunk_desc const &desc);

int decode_one_file_sw(const char *fname, zmq::socket_t &sender,
											 vs::chunk_desc const &desc);

int send_one_fb(vs::feedback const & fb, zmq::socket_t &sender);

bool recv_one_fb(zmq::socket_t &s, vs::feedback * fb, bool blocking);

int recv_one_frame(zmq::socket_t & recv);

int send_one_chunk_from_db(uint8_t * buffer, size_t sz, zmq::socket_t &sender,
													 vs::chunk_desc const & cdesc, my_alloc_hint * hint);

unsigned send_chunks_from_db(MDB_env* env, MDB_dbi dbi, vs::cid_t start, vs::cid_t end, zmq::socket_t & s);

#endif //VIDEO_STREAMER_DECODER_H
