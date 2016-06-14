#ifndef _CLANG_WALKER_DB_HXX_
#define _CLANG_WALKER_DB_HXX_

#include <db_cxx.h>

#include <string>

#define REF_PROP_DECL 1
#define REF_PROP_DEF 2

/*
 * Common key header
 */
struct db_key {
	uint32_t	dk_usr_len;
	char		dk_usr[0];
} __attribute__((packed));

struct ref_data {
	uint32_t	rd_prop;
	uint32_t	rd_line;
	uint32_t	rd_column;
	uint64_t	rd_offs;
} __attribute__((packed));

class walker_db : Db {
	Dbc *dbc;
	std::string curr_usr;
private:
	void cleanup();

public:
	int invalid;

	walker_db(const char *filename, int create, int rdonly);

	virtual ~walker_db();

	int get(const char *usr,
		uint32_t *prop,
		unsigned long *line,
		unsigned long *column,
		uint64_t *offs);

	int get_next(
		uint32_t *prop,
		unsigned long *line,
		unsigned long *column,
		uint64_t *offs);

	int set(const char *usr,
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs);

	int del();

	int del(const char *usr,
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs);
};

#endif // _CLANG_WALKER_DB_HXX_

