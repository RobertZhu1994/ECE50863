//
// Created by xzl on 12/30/17.
//

#include "config.h"
#include "msgfmt.h"

using namespace vs;

stream_desc all_streams[] = {
		{
				stream_id : 0,
				.db_path = DB_PATH,
				is_encoded: true,
				width : 320,
				height : 240,
				fps: 10,
				yuv_mode : -1,
				start : 0,
				// duration?
		},
		{
				stream_id : 1,
				db_path : {DB_RAW_FRAME_PATH},
				is_encoded: false,
				width : 320,
				height : 240,
				fps: 10,
				yuv_mode : 420,
				start : 0,
				// duration?
		},
		/* last one must have stream_id == -1 */
		{
				.stream_id = -1
		}
};

