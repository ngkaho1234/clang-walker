#include <stdint.h>

#include "clang-walker.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <string>

static int symbol_del(
		const char *usr,
		walker_ref *ref,
		void *args)
{
	walker_db *db = (walker_db *)args;
	return db->del(usr, ref);
}

static void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s <Database File> <Arguments passed to libclang>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	int n;
	CXIndex cxindex;
	CXTranslationUnit tu;
	walker_db *db;
	struct walker visitor;

	if (argc < 3)
		usage(argc, argv);

	cxindex = clang_createIndex(0, 0);
	tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 2, argc - 2, // Skip over dbFilename
			0, 0,
			CXTranslationUnit_None);
	n = clang_getNumDiagnostics(tu);
	db = new walker_db(argv[1], 1, 0);
	if (db->invalid) {
		fprintf(stderr, "Failed to open %s\n", argv[1]);
		delete db;
		exit(1);
	}

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

	visitor.symbol_op = symbol_del;
	visitor.args = db;
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		walker_visitor,
		&visitor);

	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
	delete db;
	return 0;
}
