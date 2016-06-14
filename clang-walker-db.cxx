#include <stdint.h>

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

static int walker_db_key_cmp(
		DB *db,
		const DBT *dbt1,
		const DBT *dbt2);

void walker_db::cleanup()
{
	if (!invalid) {
		if (dbc) {
			dbc->close();
			dbc = NULL;
		}

		close(0);
	}

	invalid = 1;
}

walker_db::walker_db(const char *filename, int create, int rdonly) :
	Db(NULL, DB_CXX_NO_EXCEPTIONS), invalid(0), dbc(NULL)
{
	int ret;
	ret = set_bt_compare(walker_db_key_cmp);
	if (ret) {
		invalid = 1;
		return;
	}
	ret = set_flags(DB_DUP);
	if (ret) {
		cleanup();
		return;
	}
	ret = open(
		NULL,
		filename,
		NULL,
		DB_BTREE,
		(create ? DB_CREATE : 0) |
		(rdonly ? DB_RDONLY : 0),
		0644);
	if (ret) {
		cleanup();
		return;
	}
	ret = cursor(NULL, &dbc, DB_CURSOR_BULK);
	if (ret) {
		cleanup();
		return;
	}
}

walker_db::~walker_db()
{
	cleanup();
}

int walker_db::get(
		const char *usr,
		uint32_t *prop,
		unsigned long *line,
		unsigned long *column,
		uint64_t *offs)
{
	Dbt dbt_key, dbt_data;
	int ret = 0;
	struct db_key *key;
	struct ref_data *data;
	size_t usr_len = strlen(usr);
	size_t key_len = sizeof(struct db_key) + usr_len;

	key = (struct db_key *)malloc(key_len);
	if (!key)
		return -ENOMEM;

	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, usr, usr_len);

	dbt_key.set_data(key);
	dbt_key.set_size(key_len);

	ret = dbc->get(&dbt_key, &dbt_data, DB_SET);
	if (ret)
		goto out;

	data = (struct ref_data *)dbt_data.get_data();
	assert(data);
	if (prop)
		*prop = data->rd_prop;
	if (line)
		*line = data->rd_line;
	if (column)
		*column = data->rd_column;
	if (offs)
		*offs = data->rd_offs;

out:
	free(key);
	if (!ret)
		curr_usr = usr;

	return ret;
}

int walker_db::get_next(
		uint32_t *prop,
		unsigned long *line,
		unsigned long *column,
		uint64_t *offs)
{
	Dbt dbt_key;
	Dbt dbt_data;
	int ret = 0;
	struct ref_data *data;
	struct db_key *key;
	size_t usr_len = curr_usr.length();
	size_t key_len = sizeof(struct db_key) + usr_len;

	key = (struct db_key *)malloc(key_len);
	if (!key)
		return -ENOMEM;

	if (curr_usr.empty())
		return DB_KEYEMPTY;

	dbt_key.set_data(key);
	dbt_key.set_size(key_len);

	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, curr_usr.c_str(), usr_len);

	ret = dbc->get(&dbt_key, &dbt_data, DB_NEXT_DUP);
	if (ret)
		goto out;

	data = (struct ref_data *)dbt_data.get_data();
	assert(data);
	if (prop)
		*prop = data->rd_prop;
	if (line)
		*line = data->rd_line;
	if (column)
		*column = data->rd_column;
	if (offs)
		*offs = data->rd_offs;

out:
	free(key);
	return ret;
}

int walker_db::set(
		const char *usr,
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs)
{
	Dbt dbt_key, dbt_data, dump;
	int ret = 0, notfound = 0;
	struct db_key *key;
	struct ref_data data;
	size_t usr_len = strlen(usr);
	size_t key_len = sizeof(struct db_key) + usr_len;
	if (prop & REF_PROP_DEF)
		prop |= REF_PROP_DECL;

	key = (struct db_key *)malloc(key_len);
	if (!key)
		return -ENOMEM;

	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, usr, usr_len);

	dbt_key.set_data(key);
	dbt_key.set_size(key_len);

	ret = dbc->get(&dbt_key, &dump, DB_SET);
	if (ret == DB_NOTFOUND) {
		notfound = 1;
		ret = 0;
	}
	if (ret)
		goto out;

	data.rd_prop = prop;
	data.rd_line = line;
	data.rd_column = column;
	data.rd_offs = offs;

	dbt_data.set_data(&data);
	dbt_data.set_size(sizeof(data));

	if (prop & REF_PROP_DEF || notfound)
		ret = dbc->put(&dbt_key, &dbt_data, DB_KEYFIRST);
	else
		ret = dbc->put(&dbt_key, &dbt_data, DB_AFTER);

out:
	free(key);
	if (!ret)
		curr_usr = usr;

	return ret;
}

int walker_db::del()
{
	int ret = 0;
	ret = dbc->del(0);
	if (!ret)
		curr_usr.clear();

	return ret;
}

int walker_db::del(
		const char *usr,
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs)
{
	Dbt dbt_key, dbt_data;
	int ret = 0;
	uint32_t prop_r;
	unsigned long line_r, column_r;
	uint64_t offs_r;

	ret = get(usr, &prop_r, &line_r, &column_r, &offs_r);
	while (!ret) {
		if (prop_r == prop &&
				line_r == line &&
				column_r == column &&
				offs_r == offs)
			break;

		ret = get_next(&prop_r, &line_r, &column_r, &offs_r);
	}
	if (ret)
		goto out;

	ret = del();
out:
	return ret;
}

/*
 * Walker DB comparsion function.
 * This routine compares the db_key structure,
 *
 * @db: The database handle
 * @dbt1: The caller-supplied key
 * @dbt2: The in-tree key
 */
static int walker_db_key_cmp(
		DB *db,
		const DBT *dbt1,
		const DBT *dbt2)
{
	int res;
	struct db_key *key1 = (struct db_key *)dbt1->data;
	struct db_key *key2 = (struct db_key *)dbt2->data;

	(void)db;

	if (key1->dk_usr_len < key2->dk_usr_len)
		return -1;

	if (key1->dk_usr_len > key2->dk_usr_len)
		return 1;

	res = memcmp(key1->dk_usr, key2->dk_usr, key1->dk_usr_len);
	return res;
}
