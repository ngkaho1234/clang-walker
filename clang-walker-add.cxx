#include <stdint.h>

#include "clang-walker.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <string>

static int symbol_add(
		const char *usr,
		walker_ref *ref,
		void *args)
{
	walker_db *db = (walker_db *)args;
	return db->set(usr, ref);
}

static void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <Database File> "
			"<The output AST> "
			"<Arguments passed to libclang>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	int n;
	int nr_errors = 0;
	CXIndex cxindex;
	CXTranslationUnit tu;
	walker_db *db;
	struct walker visitor;
	const char *db_fname = argv[1];
	const char *ast_fname = argv[2];

	if (argc < 4)
		usage(argc, argv);

	cxindex = clang_createIndex(0, 0);
	tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 3, argc - 3, // Skip over dbFilename and AST
			0, 0,
			CXTranslationUnit_None);

	db = new walker_db(db_fname, 1, 0);
	if (db->invalid) {
		fprintf(stderr, "Failed to open %s\n", db_fname);
		goto out;
	}

	n = clang_getNumDiagnostics(tu);
	if (n > 0) {
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

	if (!nr_errors) {
		int ret;
		ret = clang_saveTranslationUnit(tu, ast_fname, CXSaveTranslationUnit_None);
		if (ret != CXSaveError_None) {
			fprintf(stderr, "Failed to save AST to %s. Return value: %d\n",
				ast_fname, ret);
			goto out;
		}
	}

	visitor.symbol_op = symbol_add;
	visitor.args = db;
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		walker_visitor,
		&visitor);

out:
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
	delete db;
	return 0;
}
