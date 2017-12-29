//
// Created by xzl on 12/29/17.
//

#include <lmdb.h>
#include <zmq.hpp>

#include "msgfmt.h"
#include "mydecoder.h"
#include "mm.h"
#include "log.h"

using namespace vs;

/* no tx is for the db is alive as of now
 *
 * @dbi: must be opened already.
 *
 * @start/end: inclusive/exclusive
 *
 * @return: total chunks sent.
 * */
unsigned send_chunks_from_db(MDB_env* env, MDB_dbi dbi, cid_t start, cid_t end, zmq::socket_t & s)
{
	MDB_txn *txn;
	MDB_val key, data;
	MDB_cursor *cursor;
	MDB_cursor_op op;
	int rc;

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_cursor_open(txn, dbi, &cursor);
	xzl_bug_on(rc != 0);

	my_alloc_hint *hint = new my_alloc_hint(USE_LMDB_REFCNT);
	hint->txn = txn;
	hint->cursor = cursor;

	int cnt = 0;

	/* once we start to send, the refcnt can be dec by the async sender, which means it can go neg. */
	while (1) {
		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
		if (rc == MDB_NOTFOUND)
			break;

		cid_t id = *(cid_t *)key.mv_data;

		if (id >= end)
			break;

		I("got one k/v. key: %lu, sz %zu data sz %zu",
			*(uint64_t *)key.mv_data, key.mv_size,
			data.mv_size);

		chunk_desc desc;
		desc.id = id;
		desc.size = key.mv_size;
		/* XXX more */

		send_one_chunk_from_db((uint8_t *)data.mv_data, data.mv_size, s, desc, hint);
		cnt ++;
	}

	/* bump refcnt one time */
	int before = hint->refcnt.fetch_add(cnt);
	xzl_bug_on(before < - cnt ); /* impossible */

	if (before == - cnt) { /* meaning that refcnt reaches 0... all outstanding chunks are sent */
		W("close the tx");
		mdb_cursor_close(cursor);
		mdb_txn_abort(txn);
	}

	return cnt;
}
