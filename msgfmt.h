//
// Created by xzl on 12/24/17.
//

/* serialization code from boost example
 *
 * NOTE: if the serialization becomes a perf hotspot, try YAS
 * https://github.com/thekvs/cpp-serializers
 * https://github.com/niXman/yas
 *
 * */

#ifndef VIDEO_STREAMER_MSGFMT_H
#define VIDEO_STREAMER_MSGFMT_H

#include <memory> // shared_ptr

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <zmq.hpp>

namespace vs {

//	using cid_t = uint64_t; /* chunk id type */

	/* can use bit field to adjust field width. e.g.
	 * unsigned int stream_id :
	 *
	 * NB: the byte order also affects how lmdb compares keys
	 */

	using ts_t = uint32_t; /* in ms */

	struct cid_t {
		union {
			struct {
				uint32_t stream_id; /* global */
				ts_t ts;
			};
			uint64_t as_uint;
		};

		cid_t (uint64_t const & x) : as_uint(x) { }
		cid_t () { }
	};

#if 0
	struct data_desc {
	private:
		friend class boost::serialization::access;

		// When the class Archive corresponds to an output archive, the
		// & operator is defined similar to <<.  Likewise, when the class Archive
		// is a type of input archive the & operator is defined similar to >>.
		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & id;
			ar & length_ms;
			ar & size;
		}

	public:

		uint64_t id;
		uint64_t length_ms;
		uint64_t size; /* chunk size, in bytes */

	public:
		data_desc() {};

		data_desc(uint64_t key, uint64_t length, uint64_t sz) :
				id(key), length_ms(length), size(sz) {}
	};
#endif

#if 1
	enum data_type {
		TYPE_CHUNK = 0,
		TYPE_RAW_FRAME,
		TYPE_INVALID
	};

	struct data_desc {

	/* each frame is often hundreds KB. so tens B desc should be fine */

	public:
		int type;  /* enum data_type */
		cid_t cid;  /* chunk id */

		/* for TYPE_RAW_FRAME */
		int fid;    /* frame id. -1 means invalid, there's no more frame */
		int width, height;
		int yuv_mode; /* 420/422/444 */

		/* for TYPE_CHUNK */
		int fps; /* used to derive fid */
		int length_ms;

	private:
		friend class boost::serialization::access;

		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & type;
			ar & cid.as_uint;

			ar & fid;
			ar & width;
			ar & height;
			ar & yuv_mode;

			ar & length_ms;
		}

	public:
		/* must specify type */
		data_desc(int type) : type (type) {};

		data_desc() : type (TYPE_INVALID) {};

	};
#endif

	/* desc for a substream */
	struct stream_desc {
		int stream_id;	/* global id for this stream. -1 means invalid */

		std::string db_path; /* in fs */

		bool is_encoded;

		int width, height;
		int fps;

		/* for raw video */
		int yuv_mode; /* 420/422/444. */

		ts_t start, duration;

		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & stream_id;
			ar & db_path;		// don't need to send this over?

			ar & width;
			ar & height;
			ar & fps;

			ar & yuv_mode;

			ar & start;
			ar & duration;
		}
	};

/* work progress feedback from downstream */
	enum fb_type {
		FB_CHUNK_DECODED = 0,
		FB_FRAME_CONSUMED
	};

	struct feedback {
		enum fb_type type;

		cid_t cid;
		int fid;

		/* todo: more */

		feedback() {};

		feedback(cid_t cid, int fid) : cid(cid), fid(fid) {};

		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & cid.as_uint;
			ar & fid;
		}
	};

} // namespace vs

std::shared_ptr<zmq::message_t> recv_one_chunk(zmq::socket_t &s,
																							 vs::data_desc *desc);

void recv_one_chunk_to_buf(zmq::socket_t & s, vs::data_desc *desc,
													 char **p, size_t *sz);

void recv_one_chunk_tofile(zmq::socket_t & s, vs::data_desc *desc,
													 const char * fname);

#endif //VIDEO_STREAMER_MSGFMT_H
