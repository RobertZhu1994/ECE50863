//
// Created by xzl on 12/24/17.
//

/* serialization code from boost example */

#ifndef VIDEO_STREAMER_MSGFMT_H
#define VIDEO_STREAMER_MSGFMT_H

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

struct chunk_desc {
private:
	friend  class boost::serialization::access;

	// When the class Archive corresponds to an output archive, the
	// & operator is defined similar to <<.  Likewise, when the class Archive
	// is a type of input archive the & operator is defined similar to >>.
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & key;
		ar & length_ms;
	}

public:

	uint64_t key;
	uint64_t length_ms;

public:
	chunk_desc(){};
	chunk_desc(uint64_t k, uint64_t l) :
			key(k), length_ms(l)
	{}
};

struct frame_desc {


	uint64_t chunk_key;
	uint64_t frame_id;
};

#endif //VIDEO_STREAMER_MSGFMT_H
