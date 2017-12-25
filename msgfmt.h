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
		ar & id;
		ar & length_ms;
		ar & size;
	}

public:

	uint64_t id;
	uint64_t length_ms;
	uint64_t size; /* chunk size, in bytes */

public:
	chunk_desc(){};
	chunk_desc(uint64_t k, uint64_t l, uint64_t s) :
			id(k), length_ms(l), size(s)
	{}
};

struct frame_desc {

private:
	friend  class boost::serialization::access;

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & cid;
		ar & fid;
	}

public:
	uint64_t cid;
	uint64_t fid;

public:
	frame_desc() {};
	frame_desc(uint64_t cid, uint64_t fid) :
	 cid (cid), fid(fid) { };

};

#endif //VIDEO_STREAMER_MSGFMT_H
