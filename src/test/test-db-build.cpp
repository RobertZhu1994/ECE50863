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
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>

#include <lmdb.h>

#include "config.h"
#include "log.h"
#include "mm.h"

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

char input_dir[] = "/shared/videos/seg/";

/* return a vector of full path string */
#include <dirent.h>
#include <vs-types.h>

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

#define DB_NAME_CHUNK "chunk"

void build_chunk_db(void)
{
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

	rc = mdb_env_open(env, DB_PATH, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, 0, &txn);
	xzl_bug_on(rc != 0);

	/* MDB_INTEGERKEY must be specified at build time */
//	rc = mdb_dbi_open(txn, DB_NAME_CHUNK, MDB_INTEGERKEY | MDB_CREATE, &dbi);
	rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY | MDB_CREATE, &dbi);
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
		uint64_t realkey = rand() % INT_MAX;

		key.mv_data = &realkey;
		key.mv_size = sizeof(realkey);

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
}

/* config */
#define LMDB_RAWFRAME_PATH "/shared/videos/lmdb-rf/"
#define DB_NAME_RAW_FRAMES "raw_frames"

//char input_raw_video[] = "/shared/videos/raw-320x240-yuv420p.yuv";
//char input_raw_video[] = "/shared/videos/miami40min.yuv";
char input_raw_video[] = "/shared/videos/alpr1280x720.yuv";

static int height = 720;
static int width = 1280;
static int yuv_mode = 420;
static int fps = 30;

/* @fname: the raw video file.
 * each k/v is a frame; v is often hundred KB */
void build_raw_frame_db(const char *fname)
{
	int rc;

	size_t frame_sz = (size_t) width * (size_t) height;
	size_t frame_w;

	if (yuv_mode == 420)
		frame_w = ((frame_sz * 3) / 2);
	else if (yuv_mode == 422)
		frame_w = (frame_sz * 2);
	else if (yuv_mode == 444)
		frame_w = (frame_sz * 3);
	else
		xzl_bug("unsupported yuv");

	/* map file */
	uint8_t * buf = nullptr;
	size_t sz;
	map_file(fname, &buf, &sz);
	xzl_bug_on(!buf);
	auto n_frames = sz / frame_w;  /* # of frames we have */
	xzl_bug_on(n_frames == 0);

	srand (time(NULL));

	/* open db */
	MDB_env *env;
	MDB_dbi dbi;
	MDB_val key, data;
	MDB_txn *txn;
	MDB_stat mst;
	MDB_cursor *cursor;

	rc = mdb_env_create(&env);
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_mapsize(env, 1UL * 1024UL * 1024UL * 1024UL * 1024UL); /* 1 GiB */
	xzl_bug_on(rc != 0);

	rc = mdb_env_set_maxdbs(env, 1); /* required for named db */
	xzl_bug_on(rc != 0);

    /* build lmdb to the path */
	//EE("going to open db path %s", DB_RAW_FRAME_PATH);
	//rc = mdb_env_open(env, DB_RAW_FRAME_PATH, 0, 0664);
    //xzl_bug_on(rc != 0);

    /* build lmdb to the path */
    EE("going to open db path %s", DB_RAW_FRAME_PATH_720);
    rc = mdb_env_open(env, DB_RAW_FRAME_PATH_720, 0, 0664);
	xzl_bug_on(rc != 0);

	rc = mdb_txn_begin(env, NULL, 0, &txn);
	xzl_bug_on(rc != 0);

	/* MDB_INTEGERKEY must be specified at build time */
//	rc = mdb_dbi_open(txn, DB_NAME_RAW_FRAMES, MDB_INTEGERKEY | MDB_CREATE, &dbi);
	rc = mdb_dbi_open(txn, NULL, MDB_INTEGERKEY | MDB_CREATE, &dbi);
	xzl_bug_on(rc != 0);

	/* get the last key */
	vs::cid_t  last_key;
	rc = mdb_cursor_open(txn, dbi, &cursor);
	xzl_bug_on(rc != 0);
	rc = mdb_cursor_get(cursor, &key, &data, MDB_LAST);
	if (rc == MDB_NOTFOUND)
		last_key = 0;
	else {
		xzl_bug_on(rc != 0);
		last_key = *(vs::cid_t *) key.mv_data;
	}

	for (auto i = 0u; i < n_frames; i++) {
		last_key.as_uint += (60 / fps);
//		uint64_t realkey = rand() % INT_MAX;

		key.mv_data = &last_key;
		key.mv_size = sizeof(last_key);

		data.mv_data = buf + i * frame_w;
		data.mv_size = frame_w;

		rc = mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE);
		xzl_bug_on(rc != 0);

	}

	rc = mdb_txn_commit(txn);
	rc = mdb_env_stat(env, &mst);

	EE("lmdb stat: ms_depth %u ms_entries %zu", mst.ms_depth, mst.ms_entries);

	/* clean up */

	mdb_dbi_close(env, dbi);
	mdb_env_close(env);

	rc = munmap(buf, sz);
	xzl_bug_on(rc != 0);

}

int main() {

//	build_chunk_db();
	build_raw_frame_db(input_raw_video);

	return 0;
}
