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
	uint32_t prop;
	unsigned long line, column;
	uint64_t offs;
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
		&prop,
		&line,
		&column,
		&offs);
	while (!ret) {
		printf("USR: %s, "
			"prop: %u, "
			"line: %lu, column: %lu, offs: %llu\n",
			argv[2],
			prop,
			line, column, offs);
		ret = db->get_next(&prop, &line, &column, &offs);
	}

	delete db;
	return 0;
}
