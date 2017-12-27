//
// Created by xzl on 12/24/17.
//

/* serialization code from boost example */

#ifndef VIDEO_STREAMER_MSGFMT_H
#define VIDEO_STREAMER_MSGFMT_H

#include <memory> // shared_ptr

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace vstreamer {
	struct chunk_desc {
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
		chunk_desc() {};

		chunk_desc(uint64_t key, uint64_t length, uint64_t sz) :
				id(key), length_ms(length), size(sz) {}
	};

	struct frame_desc {

	private:
		friend class boost::serialization::access;

		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & cid;
			ar & fid;
		}

	public:
		uint64_t cid;
		int fid;	/* -1 means there's no more frame */

	public:
		frame_desc() {};

		frame_desc(uint64_t cid, int fid) :
				cid(cid), fid(fid) {};

	};

/* work progress feedback from downstream */
	enum fb_type {
		FB_CHUNK_DECODED = 0,
		FB_FRAME_CONSUMED
	};

	struct feedback {
		enum fb_type type;

		uint64_t cid;
		int fid;

		/* todo: more */

		feedback() {};

		feedback(uint64_t cid, int fid) : cid(cid), fid(fid) {};

		template<class Archive>
		void serialize(Archive &ar, const unsigned int version) {
			ar & cid;
			ar & fid;
		}
	};
} // namespace vstreamer

std::shared_ptr<zmq::message_t> recv_one_chunk(zmq::socket_t &s,
																							 vstreamer::chunk_desc *desc);

void recv_one_chunk_to_buf(zmq::socket_t & s, vstreamer::chunk_desc *desc,
													 char **p, size_t *sz);

void recv_one_chunk_tofile(zmq::socket_t & s, vstreamer::chunk_desc *desc,
													 const char * fname);

#endif //VIDEO_STREAMER_MSGFMT_H
