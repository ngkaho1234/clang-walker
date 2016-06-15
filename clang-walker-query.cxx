#include <stdint.h>

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

static void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <Database File> <USR>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	int ret;
	walker_db *db;
	walker_ref ref;
	struct ref_data *ptr;
	if (argc != 3)
		usage(argc, argv);

	db = new walker_db(argv[1], 0, 1);
	if (db->invalid) {
		fprintf(stderr, "Failed to open %s\n", argv[1]);
		delete db;
		exit(1);
	}

	ret = db->get(
		argv[2],
		&ref);
	while (!ret) {
		std::string fname;
		ptr = ref.get_data();
		ref.get_fname(&fname);
		printf("USR: %s, "
			"prop: %u, "
			"line: %u, column: %u, offs: %llu, "
			"fname: %s\n",
			argv[2],
			ptr->rd_prop,
			ptr->rd_line, ptr->rd_column, ptr->rd_offs,
			fname.c_str());
		ret = db->get_next(&ref);
	}

	delete db;
	return 0;
}
