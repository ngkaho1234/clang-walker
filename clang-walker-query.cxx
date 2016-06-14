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

using namespace std;


static void usage()
{
	fprintf(stderr, "query <Database File> <USR>\n");
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
		usage();

	db = new walker_db(argv[1], 0, 1);
	if (db->invalid)
		goto out;

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
out:
	delete db;
	return 0;
}
