#include <stdint.h>

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

walker_ref::walker_ref(
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs,
		std::string *fname) : data_len(0), data(NULL)
{
	assign(prop, line, column, offs, fname);
}

walker_ref::walker_ref() : data_len(0), data(NULL) {}

walker_ref::~walker_ref()
{
	if (data)
		free(data);
}

void walker_ref::get_fname(std::string *fname)
{
	if (data)
		fname->assign(data->rd_fname, data->rd_fname_len);
	else
		fname->clear();
}

void walker_ref::assign(
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs,
		std::string *fname)
{
	size_t fname_len = fname->length();
	size_t ref_len = sizeof(struct ref_data) + fname_len;
	struct ref_data *mem = (struct ref_data *)malloc(ref_len);

	assert(mem);
	if (data)
		free(data);

	data = mem;
	data_len = ref_len;

	data->rd_prop = prop;
	data->rd_line = line;
	data->rd_column = column;
	data->rd_offs = offs;
	data->rd_fname_len = fname_len;
	memcpy(data->rd_fname, fname->c_str(), fname_len);
}

void walker_ref::assign(struct ref_data *ref)
{
	size_t ref_len = sizeof(struct ref_data) + ref->rd_fname_len;
	struct ref_data *mem = (struct ref_data *)malloc(ref_len);

	assert(mem);
	if (data)
		free(data);

	data = mem;
	data_len = ref_len;
	memcpy(data, ref, ref_len);
}

walker_ref::walker_ref(struct ref_data *ref) : data_len(0), data(NULL)
{
	assign(ref);
}

bool walker_ref::operator ==(walker_ref &ref)
{
	struct ref_data *tmp = ref.get_data();
	if (!tmp && !data)
		return true;

	if (!tmp && data)
		return false;

	if (tmp && !data)
		return false;

	if (tmp->rd_prop == data->rd_prop &&
			tmp->rd_line == data->rd_line &&
			tmp->rd_column == data->rd_column &&
			tmp->rd_fname_len == data->rd_fname_len &&
			!memcmp(tmp->rd_fname,
				data->rd_fname,
				data->rd_fname_len)) {
		return true;
	}

	return false;
}

void walker_db::cleanup()
{
	if (!invalid) {
		clear_dbc();
		close(0);
	}

	invalid = 1;
}

walker_db::walker_db(const char *filename, int create, int rdonly) :
	Db(NULL, DB_CXX_NO_EXCEPTIONS), dbc(NULL), invalid(0)
{
	int ret;
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
}

walker_db::~walker_db()
{
	cleanup();
}

int walker_db::get(
		const char *usr,
		walker_ref *ref)
{
	Dbc *new_dbc = NULL;
	Dbt dbt_key, dbt_ret, dbt_dump;
	int ret = 0;
	struct db_key *key;
	void *key_res;
	size_t usr_len = strlen(usr);
	size_t key_len = sizeof(struct db_key) + usr_len;

	clear_dbc();

	key = (struct db_key *)malloc(key_len);
	if (!key)
		return -ENOMEM;

	ret = cursor(NULL, &new_dbc, DB_CURSOR_BULK);
	if (ret)
		goto out;

	key->dk_hdr.kh_type = KEY_USR;
	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, usr, usr_len);

	dbt_key.set_data(key);
	dbt_key.set_size(key_len);

	ret = new_dbc->get(&dbt_key, &dbt_dump, DB_SET_RANGE);
	if (ret)
		goto out;

	new_dbc->get(&dbt_ret, &dbt_dump, DB_CURRENT);
	key_res = (struct ref_data *)dbt_ret.get_data();
	assert(key_res);
	if (dbt_ret.get_size() < key_len) {
		ret = DB_NOTFOUND;
		goto out;
	}
	if (memcmp(key_res, key, key_len)) {
		ret = DB_NOTFOUND;
		goto out;
	}

	ref->assign((struct ref_data *)((char *)key_res + key_len));

out:
	free(key);
	if (!ret) {
		std::string usr_str = usr;
		set_dbc(new_dbc, usr_str);
	} else {
		if (new_dbc)
			new_dbc->close();

	}

	return ret;
}

int walker_db::get_exact(
		const char *usr,
		walker_ref *ref)
{
	Dbc *new_dbc = NULL;
	Dbt dbt_key, dbt_ret, dbt_dump;
	int ret = 0;
	struct db_key *key;
	void *key_res;
	size_t usr_len = strlen(usr);
	size_t key_len = sizeof(struct db_key) + usr_len;
	size_t total_len = key_len + ref->get_size();

	clear_dbc();

	key_res = key = (struct db_key *)malloc(total_len);
	if (!key)
		return -ENOMEM;

	memset(key_res, 0, total_len);
	ret = cursor(NULL, &new_dbc, DB_CURSOR_BULK);
	if (ret)
		goto out;

	key->dk_hdr.kh_type = KEY_USR;
	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, usr, usr_len);
	if (ref->get_size())
		memcpy((char *)key_res + key_len, ref->get_data(),
			ref->get_size());

	dbt_key.set_data(key);
	dbt_key.set_size(total_len);

	ret = new_dbc->get(&dbt_key, &dbt_dump, DB_SET);
	if (ret)
		goto out;

out:
	free(key);
	if (!ret) {
		std::string usr_str = usr;
		set_dbc(new_dbc, usr_str);
	} else {
		if (new_dbc)
			new_dbc->close();

	}

	return ret;
}

int walker_db::get_next(walker_ref *ref)
{
	Dbt dbt_key, dbt_dump;
	int ret = 0;
	void *key_res;
	struct db_key *key;
	size_t usr_len = curr_usr.length();
	size_t key_len = sizeof(struct db_key) + usr_len;

	if (!dbc || curr_usr.empty()) {
		clear_dbc();
		return DB_NOTFOUND;
	}

	ret = dbc->get(&dbt_key, &dbt_dump, DB_NEXT);
	if (ret)
		goto out;

	key_res = (struct ref_data *)dbt_key.get_data();
	assert(key_res);
	key = (struct db_key *)key_res;
	if (key->dk_hdr.kh_type != KEY_USR) {
		ret = DB_NOTFOUND;
		goto out;
	}
	if (usr_len != key->dk_usr_len) {
		ret = DB_NOTFOUND;
		goto out;
	}
	if (memcmp(key->dk_usr, curr_usr.c_str(), usr_len)) {
		ret = DB_NOTFOUND;
		goto out;
	}

	ref->assign((struct ref_data *)((char *)key_res + key_len));
out:
	return ret;
}

int walker_db::set(
		const char *usr,
		walker_ref *ref)
{
	Dbc *new_dbc = NULL;
	Dbt dbt_key, dbt_fname_key, dbt_dump;
	int ret = 0;
	struct db_key *key;
	struct db_key_fname *key_fname;
	void *key_res, *key_fname_res;
	size_t usr_len = strlen(usr);
	size_t key_len = sizeof(struct db_key) + usr_len;
	size_t total_len = key_len + ref->get_size();

	size_t fname_len;
	size_t key_fname_len;
	size_t total_fname_len;
	std::string fname;
	ref->get_fname(&fname);
	fname_len = fname.length();
	key_fname_len = sizeof(struct db_key_fname) + fname_len;
	total_fname_len = key_fname_len + total_len;

	clear_dbc();

	key_res = key = (struct db_key *)malloc(total_len);
	if (!key)
		return -ENOMEM;

	key_fname_res = key_fname =
		(struct db_key_fname *)malloc(total_fname_len);
	if (!key) {
		free(key);
		return -ENOMEM;
	}

	memset(key_res, 0, total_len);
	ret = cursor(NULL, &new_dbc, DB_CURSOR_BULK);
	if (ret)
		goto out;

	key->dk_hdr.kh_type = KEY_USR;
	key->dk_usr_len = usr_len;
	memcpy(key->dk_usr, usr, usr_len);
	if (ref->get_size())
		memcpy((char *)key_res + key_len, ref->get_data(),
			ref->get_size());

	dbt_key.set_data(key);
	dbt_key.set_size(total_len);

	key_fname->dk_hdr.kh_type = KEY_FNAME;
	key_fname->dk_fname_len = fname_len;
	memcpy(key_fname->dk_fname, fname.c_str(), fname_len);
	memcpy((char *)key_fname_res + key_fname_len,
		key,
		total_len);

	dbt_fname_key.set_data(key_fname);
	dbt_fname_key.set_size(total_fname_len);

	dbt_dump.set_size(0);

	ret = new_dbc->put(&dbt_key, &dbt_dump, DB_KEYLAST);
	if (ret)
		goto out;

	ret = new_dbc->put(&dbt_fname_key, &dbt_dump, DB_KEYLAST);
	if (ret)
		goto out;

out:
	free(key);
	free(key_fname);
	if (new_dbc)
		new_dbc->close();

	return ret;
}

int walker_db::del_fname(const char *fname)
{
	Dbc *new_dbc = NULL;
	Dbt dbt_fname_key, dbt_ret, dbt_dump;
	int ret = 0;
	struct db_key_fname *key_fname = NULL;
	void *key_res;
	size_t fname_len = strlen(fname);
	size_t key_fname_len = sizeof(struct db_key_fname) + fname_len;

	clear_dbc();

	key_fname = (struct db_key_fname *)malloc(key_fname_len);
	if (!key_fname)
		return -ENOMEM;

	ret = cursor(NULL, &new_dbc, DB_CURSOR_BULK);
	if (ret)
		goto out;

	key_fname->dk_hdr.kh_type = KEY_FNAME;
	key_fname->dk_fname_len = fname_len;
	memcpy(key_fname->dk_fname, fname, fname_len);

	dbt_fname_key.set_data(key_fname);
	dbt_fname_key.set_size(key_fname_len);

	while (!ret) {
		Dbc *del_dbc = NULL;
		Dbt dbt_key;
		void *key;
		size_t key_len;

		ret = new_dbc->get(&dbt_fname_key, &dbt_dump, DB_SET_RANGE);
		if (ret)
			break;

		new_dbc->get(&dbt_ret, &dbt_dump, DB_CURRENT);
		key_res = (struct ref_data *)dbt_ret.get_data();
		assert(key_res);
		if (dbt_ret.get_size() < key_fname_len)
			break;

		if (memcmp(key_res, key_fname, key_fname_len))
			break;

		key_len = dbt_ret.get_size() - key_fname_len;
		key = malloc(key_len);
		if (!key)
			break;

		memcpy(key, (char *)dbt_ret.get_data() + key_fname_len,
				key_len);

		ret = cursor(NULL, &del_dbc, DB_CURSOR_BULK);
		if (ret) {
			free(key);
			goto out;
		}

		dbt_key.set_data(key);
		dbt_key.set_size(key_len);

		ret = del_dbc->get(&dbt_key, &dbt_dump, DB_SET);
		if (!ret)
			del_dbc->del(0);

		del_dbc->close();
		free(key);

		new_dbc->del(0);
	}
	if (ret == DB_NOTFOUND)
		ret = 0;

out:
	free(key_fname);
	if (new_dbc)
		new_dbc->close();

	return ret;
}
