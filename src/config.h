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
#define WORKER_REQUEST "tcp://localhost:5560"
#define WORKER_PULL_STREAMINFO_FROM_ADDR "tcp://localhost:5561"
#define WORKER720_REQUEST "tcp://localhost:5562"


#define FRAME_PULL_ADDR 						"tcp://*:5557"
#define CHUNK_PUSH_ADDR 						"tcp://*:5558"
#define FB_PULL_ADDR 								"tcp://*:5559"
#define REQUEST 								"tcp://*:5560"
#define STREAMINFO_PUSH_ADDR 				"tcp://*:5561"
#define REQUEST720 								"tcp://*:5562"

#define DB_PATH 							"/shared/videos/lmdb/"
#define DB_RAW_FRAME_PATH 		"/shared/videos/lmdb-rf/"
#define DB_RAW_FRAME_PATH_720   "/shared/videos/lmdb-rf720"
//#define DB_RAW_FRAME_PATH 		"/shared/videos/lmdb-rf-ssd/"

enum db_sequence{
    HDD_RAW_180=0,
    HDD_RAW_720,
    HDD_CHUNK_180,
    HDD_CHUNK_720
};

#endif //VIDEO_STREAMER_CONFIG_H
