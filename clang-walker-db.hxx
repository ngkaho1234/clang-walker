#ifndef _CLANG_WALKER_DB_HXX_
#define _CLANG_WALKER_DB_HXX_

#include <db_cxx.h>

#include <string>

#define KEY_USR 0

#define REF_PROP_DECL 1
#define REF_PROP_DEF 2

/*
 * Common key header
 */
struct db_key_hdr {
	uint32_t	kh_type;
};

struct db_key {
	struct db_key_hdr dk_hdr;
	uint32_t	dk_usr_len;
	char		dk_usr[0];
} __attribute__((packed));

struct ref_data {
	uint32_t	rd_prop;
	uint32_t	rd_line;
	uint32_t	rd_column;
	uint64_t	rd_offs;
	uint32_t	rd_fname_len;
	char		rd_fname[0];
} __attribute__((packed));

class walker_ref {
	size_t		 data_len;
	struct ref_data *data;

public:
	walker_ref(
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs,
		std::string *fname);

	walker_ref();

	walker_ref(struct ref_data *ref);

	struct ref_data *get_data()
	{
		return data;
	}

	size_t get_size()
	{
		return data_len;
	}

	void get_fname(std::string *fname);

	void assign(
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs,
		std::string *fname);

	void assign(struct ref_data *ref);

	bool operator ==(walker_ref &ref);

	~walker_ref();
};

class walker_db : Db {
	Dbc *dbc;
	std::string curr_usr;
	void set_dbc(Dbc *new_dbc, std::string &usr)
	{
		dbc = new_dbc;
		curr_usr = usr;
	}

	void clear_dbc()
	{
		curr_usr.clear();
		if (dbc)
			dbc->close();

		dbc = NULL;
	}

	void cleanup();

public:
	int invalid;

	walker_db(const char *filename, int create, int rdonly);

	virtual ~walker_db();

	int get(const char *usr,
		walker_ref *ref);

	int get_next(walker_ref *ref);

	int set(const char *usr,
		walker_ref *ref);

	int del();

	int del(const char *usr,
		walker_ref *ref);
};

#endif // _CLANG_WALKER_DB_HXX_

