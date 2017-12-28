//
// Created by xzl on 12/27/17.
//
/*
 g++ lmdb-build.cpp -o lmdb-build.bin -I../ -llmdb -std=c++11 -O0 -Wall -g
 *
 */
//#include <lmdb++.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "log.h"

#include "lmdb.h"

#define LMDB_PATH "/shared/videos/lmdb/"

/* load the given file contents to mem
 * @p: allocated by this func, to be free'd by caller */
void load_file(const char *fname, char **p, size_t *sz)
{
	/* get file sz */
	struct stat finfo;
	int fd = open(fname, O_RDONLY);
	xzl_bug_on(fd < 0);
	int ret = fstat(fd, &finfo);
	xzl_bug_on(ret != 0);

	char *buff = (char *)malloc(finfo.st_size);
	xzl_bug_on(!buff);

	auto s = pread(fd, buff, finfo.st_size, 0);
	xzl_bug_on(s != finfo.st_size);

	*p = buff;
	*sz = s;
}

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

	rc = mdb_env_create(&env);
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_mapsize(env, 1UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
	xzl_bug_on(rc != 0);

	rc = mdb_env_open(env, LMDB_PATH, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, 0, &txn);
	xzl_bug_on(rc != 0);

	rc = mdb_dbi_open(txn, NULL, 0, &dbi);
	xzl_bug_on(rc != 0);

	for (unsigned i = 0; i < sizeof(input_fnames) /  sizeof(char *); i++) {
		char * buf = nullptr;
		size_t sz;
		load_file(input_fnames[i], &buf, &sz);
		key.mv_data = input_fnames[i];
		key.mv_size = strnlen(input_fnames[i], 50);

		data.mv_data = buf;
		data.mv_size = sz;

		rc = mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE);
		xzl_bug_on(rc != 0);
	}

	rc = mdb_txn_commit(txn);
	rc = mdb_env_stat(env, &mst);

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	return 0;
}