#include <stdint.h>

#include <clang-c/Index.h>
#include <db_cxx.h>

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

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

static int walker_db_key_cmp(
		DB *db,
		const DBT *dbt1,
		const DBT *dbt2);

class walker_db : Db {
	int invalid;
	Dbc *dbc;
private:
	void cleanup()
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
public:
	walker_db(const char *filename) : Db(NULL, DB_CXX_NO_EXCEPTIONS), invalid(0), dbc(NULL)
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
			DB_CREATE,
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
	virtual ~walker_db()
	{
		cleanup();
	}

	int get(const char *usr,
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
		size_t key_len = sizeof(struct db_key *) + usr_len;

		key = (struct db_key *)malloc(key_len);
		if (!key)
			return -ENOMEM;

		key->dk_usr_len = usr_len;
		memcpy(key->dk_usr, usr, usr_len);

		dbt_key.set_data(key);
		dbt_key.set_size(key_len);

		ret = dbc->get(&dbt_key, &dbt_data, DB_GET_BOTH);
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

	int get_next(
		uint32_t *prop,
		unsigned long *line,
		unsigned long *column,
		uint64_t *offs)
	{
		Dbt dbt_data;
		int ret = 0;
		struct ref_data *data;

		ret = dbc->get(NULL, &dbt_data, DB_GET_BOTH);
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
		return ret;
	}

	int set(const char *usr,
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
		return ret;
	}

	int del(const char *usr,
		uint32_t prop,
		unsigned long line,
		unsigned long column,
		uint64_t offs)
	{
		Dbt dbt_key, dbt_data;
		int ret = 0;
		struct db_key *key;
		struct ref_data data;
		size_t usr_len = strlen(usr);
		size_t key_len = sizeof(struct db_key *) + usr_len;
		if (prop & REF_PROP_DEF)
			prop |= REF_PROP_DECL;

		key = (struct db_key *)malloc(key_len);
		if (!key)
			return -ENOMEM;

		key->dk_usr_len = usr_len;
		memcpy(key->dk_usr, usr, usr_len);

		dbt_key.set_data(key);
		dbt_key.set_size(key_len);

		data.rd_prop = prop;
		data.rd_line = line;
		data.rd_column = column;
		data.rd_offs = offs;

		dbt_data.set_data(&data);
		dbt_data.set_size(sizeof(data));

		ret = dbc->get(&dbt_key, &dbt_data, DB_GET_BOTH);
		if (ret)
			goto out;

		ret = dbc->del(0);

	out:
		free(key);
		return ret;
	}
};

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

enum CXChildVisitResult visitor(
		CXCursor cursor,
		CXCursor parent,
		CXClientData data)
{
	walker_db *db = (walker_db *)data;
	CXType cursor_type = clang_getCursorType(cursor);
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXCursorKind pcursor_kind = clang_getCursorKind(parent);

	CXString usr = clang_getCursorUSR(cursor);
	CXString spelling = clang_getCursorSpelling(cursor);
	CXString type_spelling = clang_getTypeKindSpelling(cursor_type.kind);
	CXString kind_spelling = clang_getCursorKindSpelling(cursor_kind);
	CXSourceLocation location = clang_getCursorLocation(cursor);
	unsigned int line, colume, offs;
	uint32_t prop = 0;

	clang_getSpellingLocation(location, NULL, &line, &colume, &offs);
	printf("USR: %s, cursor_kind: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, colume: %d, offs: %d, "
		"isReference: %d\n",
		clang_getCString(usr), clang_getCString(kind_spelling),
		clang_getCString(type_spelling), clang_getCString(spelling),
		line, colume, offs,
		clang_isReference(cursor_kind));

	if (clang_isDeclaration(cursor_kind))
		prop = REF_PROP_DECL;

	if (cursor_kind == CXCursor_CompoundStmt) {
		CXString pusr = clang_getCursorUSR(parent);
		if (clang_getCString(pusr)[0])
			db->set(clang_getCString(pusr),
				prop | REF_PROP_DECL | REF_PROP_DEF,
				line,
				colume,
				offs);

		clang_disposeString(pusr);
	} else if (clang_getCString(usr)[0]) {
		db->set(clang_getCString(usr),
			prop,
			line,
			colume,
			offs);
	}

	clang_disposeString(usr);
	clang_disposeString(type_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(spelling);
	return CXChildVisit_Recurse;
}

int main(int argc, char **argv)
{
	CXIndex cxindex = clang_createIndex(0, 0);
	CXTranslationUnit tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 1, argc - 1, // Skip over dbFilename and indexFilename
			0, 0,
			CXTranslationUnit_None);
	int n = clang_getNumDiagnostics(tu);
	walker_db *db = new walker_db("test.db");

	if (n > 0) {
		int nr_errors = 0;
		int i;
		for (i = 0; i < n; ++i) {
			CXDiagnostic diag = clang_getDiagnostic(tu, i);
			CXString string = clang_formatDiagnostic(diag,
						clang_defaultDiagnosticDisplayOptions());
			fprintf(stderr, "%s\n", clang_getCString(string));
			if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Error
					|| clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal)
				nr_errors++;
		}
	}
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		&visitor,
		db);
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
	delete db;
	return 0;
}
