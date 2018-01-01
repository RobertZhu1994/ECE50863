//
// Created by xzl on 1/1/18.
//

#ifndef VIDEO_STREAMER_RX_MANAGER_H
#define VIDEO_STREAMER_RX_MANAGER_H

#include <memory>
#include <tuple>
#include <vector>
#include <map>

#include "vs-types.h"

#include <zmq.hpp>

using namespace std;

namespace vs {

	/* used to manage recv'd frames yet to be consumed.
	 * assumption: there won't be too many frames
	 */
//	using Frame = std::tuple<vs::data_desc, std::shared_ptr<zmq::message_t>>;

	/* two level heaps. lv1: chunks; lv2: frames
	 *
	 * rationale: we'd like both quick insertion and quick iteration.
	 * considered but rejected: vector of vector, vector of deque, priority queues
	 */

	/* ------------- frames -------------- */

	struct Frame {
		vs::data_desc desc;
		std::shared_ptr<zmq::message_t> msg_p;
	};

	template<class FrameT>
	struct FrameCompGt {
		bool operator() (FrameT const & lhs, FrameT const & rhs) const {
			/* since we are going to build a min heap .. */
			xzl_assert(lhs.desc.f_seq != rhs.desc.f_seq);
			return (lhs.desc.f_seq > rhs.desc.f_seq);
		}
	};

//	using frame_heap_t = priority_queue<Frame, vector<Frame>, FrameCmpGt<Frame>>;
	using frame_map_t = map<seq_t, Frame>;

	/* ---------- chunks ------------------ */

	struct Chunk {
		unsigned int c_seq; /* we don't need other things in chunk's desc */
		long eof_f = -1; /* got eof for frames? */
		static_assert(sizeof(eof_f) > sizeof(seq_t), "must > seq_t");

		frame_map_t frames;
	};

	template<class FrameT>
	struct ChunkCompGt {
		bool operator() (Chunk const & lhs, Chunk const & rhs) const {
			/* since we are going to build a min heap .. */
			xzl_assert(lhs.c_seq != rhs.c_seq);
			return (lhs.c_seq > rhs.c_seq);
		}
	};

	/* comp two frames based on their desc. first chunk seq, then frame seq */
	template<class FrameT>
	struct ChunkFrameCmpGt {
		bool operator() (FrameT const & lhs, FrameT const & rhs) const {
			/* since we are going to build a min heap .. */
			if (lhs.desc.c_seq == rhs.msg.c_seq)
				return (lhs.desc.f_seq > rhs.desc.f_seq);
			else
				return (lhs.desc.c_seq > rhs.desc.c_seq);
		}
	};

//	using chunk_heap_t = priority_queue<Chunk, vector<Chunk>, ChunkCmpGt<Chunk>>;
	using chunk_map_t = map<seq_t, Chunk>;

	class RxManager {

//		std::priority_queue<Frame, vector<Frame>, ChunkFrameCmpGt<Frame>> queue;

	private:
//		chunk_heap_t chunks;
		chunk_map_t chunks;

		/* unknown so far until we get chunk eof */
		long eof_c = -1;
		static_assert(sizeof(eof_c) > sizeof(seq_t), "must > seq_t");

		/* watermarks */
		seq_t wm_chunk = 0, wm_frame = 0; /* the next frame that the reader should see */

	public:
		int
		DepositAFrame(data_desc const &fdesc, shared_ptr<zmq::message_t> &msg_p) {

			auto & c_seq = fdesc.c_seq;
			auto & f_seq = fdesc.f_seq;

			xzl_bug_on(!is_type_data(fdesc.type));

			/* new chunk */
			if (chunks.find(c_seq) == chunks.end()) {
				xzl_bug_on_msg(eof_c >= 0 && c_seq > eof_c, "exceed chunk eof");
				chunks[c_seq].c_seq = c_seq; /* will create the new frame map */
			}

			/* chunk exists */
			auto & frame_map = chunks[c_seq].frames;
			xzl_bug_on_msg(chunks[c_seq].eof_f >= 0, "already frame eof?");
			xzl_bug_on_msg(frame_map.find(f_seq) != frame_map.end(), "dup frames?");

			frame_map.emplace(f_seq, {.desc = fdesc, .msg_p = msg_p});

			return 0;
		}

		/* handling various eof */
		void DepositADesc(data_desc const &desc) {
			xzl_bug_on(!is_type_eof(desc.type));

			if (desc.type == TYPE_CHUNK_EOF) {
				xzl_bug_on(eof_c >= 0);
				xzl_bug_on_msg(desc.c_seq <= chunks.rend().first,
											 "eof smaller than existing chunk seq?");
				eof_c = desc.c_seq;
				return;
			}

			if (desc.type == TYPE_DECODED_FRAME_EOF ||
					desc.type == TYPE_RAW_FRAME_EOF) {
				/* new chunk -- f eof arrives before any frames. ever possible? */
				xzl_bug_on_msg(chunks.find(c_seq) == chunks.end(), "tbd");

				/* chunk exists */
				auto & chunk = chunks[c_seq];
				auto & frame_map = chunk.frames;
				xzl_bug_on_msg(chunk.eof_f >= 0, "already frame eof?");
				xzl_bug_on_msg(desc.f_seq <= frame_map.rend().first,
											 "eof smaller than existing frame seq?");
				chunk.eof_f = desc.f_seq;
				return;
			}

			xzl_bug_on("unknown desc type");
		}

		/* get and remove a frame from the manager.
		 * will update watermarks accordingly.
		 *
		 * @frame: [out]
		 *
		 * return 0: ok
		 * -1: no more chunks (we're done)
		 * -2: no more; there should be more.
		 */
		int RetrieveAFrame(Frame * frame) {
		again:
			if (wm_chunk == eof_c) { /* done! */
				xzl_bug_on(wm_frame);
				return - VS_ERR_EOF_CHUNKS;
			}

			auto it_chunk = chunks.find(wm_chunk);
			if (it_chunk == chunks.end()) {	/* chunk not arrived yet */
				xzl_bug_on(wm_frame);
				return - VS_ERR_AGAIN;
			}

			auto & chunk = chunks[wm_chunk];
			auto & frames = chunks[wm_chunk].frames;

			if (wm_frame == chunk.eof_f) {
				xzl_assert(chunk.eof_f >= 0);
				/* advance to next chunk */
				wm_chunk ++;
				wm_frame = 0;
				chunks.erase(it_chunk);
				goto again;
			} else {
				auto it = frames.find(wm_frame);
				if (it == frames.end()) {
					/* next frame does not exist yet.
					 * it may point to a future eof_f, or a future frame */
					return - VS_ERR_AGAIN;
				}

				/* frame exists */
				f->desc = it->desc;
				f->msg_p = it->msg_p;

				wm_frame ++;
				frames.erase(it);
			}

			return 0;
		}
	};
};

#endif //VIDEO_STREAMER_RX_MANAGER_H
