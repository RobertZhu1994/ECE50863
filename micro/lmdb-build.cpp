//
// Created by xzl on 12/27/17.
//
// cf> https://github.com/LMDB/lmdb/blob/mdb.master/libraries/liblmdb/mtest.c
/*
 g++ lmdb-build.cpp -o lmdb-build.bin -I../ -llmdb -std=c++11 -O0 -Wall -g
 *
 */
//#include <lmdb++.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>

#include "log.h"

#include "lmdb.h"

#define LMDB_PATH "/shared/videos/lmdb/"

using namespace std;

/* load the given file contents to mem
 * @p: allocated by this func, to be free'd by caller */
void load_file(const char *fname, char **p, size_t *sz)
{
	/* get file sz */
	I("load file %s...", fname);

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

#if 0
char * input_fnames[] = {
	"/shared/videos/seg/video-200p-10-000.mp4",
	"/shared/videos/seg/video-200p-10-001.mp4",
	"/shared/videos/seg/video-200p-10-002.mp4",
	"/shared/videos/seg/video-200p-10-003.mp4",
	"/shared/videos/seg/video-200p-10-004.mp4",
	"/shared/videos/seg/video-200p-10-005.mp4"
};
#endif

char input_dir[] = "/shared/videos/seg/";


/* return a vector of full path string */
#include <dirent.h>
vector<string> get_all_files_dir(char * dirpath)
{
	vector<string> ret;

	xzl_bug_on(!dirpath);

	DIR *dir;
	struct dirent *ent;
	string sdir(dirpath);

	if ((dir = opendir (dirpath)) != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir (dir)) != NULL) {
			if (strncmp(ent->d_name, ".", 10) == 0 || strncmp(ent->d_name, "..", 10) == 0)
				continue;
//			printf ("%s\n", ent->d_name);
			string s(ent->d_name);
			ret.push_back(sdir + s);
		}
		closedir (dir);
	} else {
		/* could not open directory */
		perror ("");
		return ret;
	}

	sort(ret.begin(), ret.end());

	return ret;
}


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

	/* MDB_INTEGERKEY must be specified at build time */
	rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi);
	xzl_bug_on(rc != 0);

	auto fnames = get_all_files_dir(input_dir);

	srand (time(NULL));

	int cnt = 0 ;
	for (auto & fname : fnames) {
//	for (unsigned i = 0; i < sizeof(input_fnames) /  sizeof(char *); i++) {
		char * buf = nullptr;
		size_t sz;
//		load_file(input_fnames[i], &buf, &sz);
		load_file(fname.c_str(), &buf, &sz);

		/* generate some random key */
//		uint64_t realkey = cnt * 4;
		uint64_t realkey = rand() % INT_MAX;

		key.mv_data = &realkey;
		key.mv_size = sizeof(realkey);

//		key.mv_data = input_fnames[i];
//		key.mv_size = strnlen(input_fnames[i], 50);

		data.mv_data = buf;
		data.mv_size = sz;

		rc = mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE);
		xzl_bug_on(rc != 0);

		cnt ++;
	}

	rc = mdb_txn_commit(txn);
	rc = mdb_env_stat(env, &mst);

	I("lmdb stat: ms_depth %u ms_entries %zu", mst.ms_depth, mst.ms_entries);

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	return 0;
}