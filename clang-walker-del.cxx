#include <stdint.h>

#include "clang-walker.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <string>

static void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <Database File> <Source file path>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	int n;
	walker_db *db;
	const char *db_fname = argv[1];
	const char *src_fname = argv[2];

	if (argc < 3)
		usage(argc, argv);

	db = new walker_db(db_fname, 1, 0);
	if (db->invalid) {
		fprintf(stderr, "Failed to open %s\n", db_fname);
		goto out;
	}

	db->del_fname(src_fname);

out:
	delete db;
	return 0;
}
