//
// Created by xzl on 12/29/17.
//

#ifndef VIDEO_STREAMER_RXTX_H
#define VIDEO_STREAMER_RXTX_H

#include <zmq.hpp>
#include "msgfmt.h"
#include "mm.h"

int send_one_fb(vs::feedback const & fb, zmq::socket_t &sender);

bool recv_one_fb(zmq::socket_t &s, vs::feedback * fb, bool blocking);

unsigned send_multi_from_db(MDB_env *env, MDB_dbi dbi, vs::cid_t start, vs::cid_t end, zmq::socket_t &s,
														vs::data_desc const &desc);

int send_one_frame(uint8_t *buffer, int size, zmq::socket_t &sender,
									 vs::data_desc const &desc);

int send_one_frame_mmap(uint8_t *buffer, size_t sz, zmq::socket_t &sender,
												vs::data_desc const & fdesc, my_alloc_hint * hint);

int send_one_from_db(uint8_t * buffer, size_t sz, zmq::socket_t &sender,
													 vs::data_desc const & cdesc, my_alloc_hint * hint);

int recv_one_frame(zmq::socket_t & recv, size_t* sz = nullptr);

int send_all_stream_info(zmq::socket_t & sender);

#endif //VIDEO_STREAMER_RXTX_H
