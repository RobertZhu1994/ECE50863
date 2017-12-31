//
// Created by xzl on 12/24/17.
//

#ifndef VIDEO_STREAMER_CONFIG_H
#define VIDEO_STREAMER_CONFIG_H

#define SERVER_PULL_ADDR "tcp://*:5557"
#define SERVER_PUSH_ADDR "tcp://*:5558"

#define CLIENT_PULL_FROM_ADDR "tcp://localhost:5558"
#define CLIENT_PUSH_TO_ADDR "tcp://localhost:5557"

/* ---- */

#define WORKER_PUSH_TO_ADDR "tcp://localhost:5557"
#define WORKER_PULL_FROM_ADDR "tcp://localhost:5558"
#define WORKER_PUSHFB_TO_ADDR "tcp://localhost:5559"

#define FRAME_PULL_ADDR "tcp://*:5557"
#define CHUNK_PUSH_ADDR "tcp://*:5558"
#define FB_PULL_ADDR "tcp://*:5559"

#define DB_PATH 							"/shared/videos/lmdb/"
#define DB_RAW_FRAME_PATH 		"/shared/videos/lmdb-rf/"
//#define DB_RAW_FRAME_PATH 		"/shared/videos/lmdb-rf-ssd/"


#endif //VIDEO_STREAMER_CONFIG_H
