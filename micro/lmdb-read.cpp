//
// Created by xzl on 12/27/17.
//
/*
 g++ lmdb-read.cpp -o lmdb-read.bin -I../ -llmdb -std=c++11 -O0 -Wall -g

 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "log.h"

#include "lmdb.h"

#define LMDB_PATH "/shared/videos/lmdb/"



char * input_fnames[] = {
		"/shared/videos/seg/video-200p-10-000.mp4"
};

int main() {

	int rc;
	MDB_env *env;
	MDB_dbi dbi;
	MDB_val key, data;
	MDB_txn *txn;
	MDB_stat mst;
	MDB_cursor *cursor, *cur2;
	MDB_cursor_op op;


	rc = mdb_env_create(&env);
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_mapsize(env, 1UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
	xzl_bug_on(rc != 0);

	rc = mdb_env_open(env, LMDB_PATH, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
	xzl_bug_on(rc != 0);

	/* dump all key/v */
	rc = mdb_cursor_open(txn, dbi, &cursor);
	xzl_bug_on(rc != 0);

	while (1) {
		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
		if (rc == MDB_NOTFOUND)
			break;
		I("got one k/v. key: %s, data sz %lu",
		(const char *)key.mv_data,
		data.mv_size);

//		printf("key: %p %.*s, data: %p %.*s\n",
//					 key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
//					 data.mv_data, (int) data.mv_size, (char *) data.mv_data);
	}

	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);
}
