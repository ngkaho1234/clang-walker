#include <stdint.h>

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

enum CXChildVisitResult visitor(
		CXCursor cursor,
		CXCursor parent,
		CXClientData data)
{
	walker_db *db = (walker_db *)data;
	CXType cursor_type = clang_getCursorType(cursor);
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXCursorKind pcursor_kind = clang_getCursorKind(parent);
	CXFile file;

	CXString fname;
	CXString usr = clang_getCursorUSR(cursor);
	CXString spelling = clang_getCursorSpelling(cursor);
	CXString type_spelling = clang_getTypeKindSpelling(cursor_type.kind);
	CXString kind_spelling = clang_getCursorKindSpelling(cursor_kind);
	CXSourceLocation location = clang_getCursorLocation(cursor);
	unsigned int line, column, offs;
	uint32_t prop = 0;
	walker_ref ref;

	clang_getSpellingLocation(location, &file, &line, &column, &offs);
	fname = clang_getFileName(file);
	printf("USR: %s, cursor_kind: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, column: %d, offs: %d, "
		"isReference: %d\n",
		clang_getCString(usr), clang_getCString(kind_spelling),
		clang_getCString(type_spelling), clang_getCString(spelling),
		line, column, offs,
		clang_isReference(cursor_kind));

	if (clang_isDeclaration(cursor_kind))
		prop = REF_PROP_DECL;

	if (cursor_kind == CXCursor_CompoundStmt) {
		CXString pusr = clang_getCursorUSR(parent);
		std::string fname_str = clang_getCString(fname);
		ref.assign(
			prop | REF_PROP_DECL | REF_PROP_DEF,
			line, column, offs, &fname_str);

		if (clang_getCString(pusr)[0])
			db->del(clang_getCString(pusr),
				&ref);

		clang_disposeString(pusr);
	} else if (clang_getCString(usr)[0]) {
		std::string fname_str = clang_getCString(fname);
		ref.assign(
			prop,
			line, column, offs, &fname_str);
		db->del(clang_getCString(usr),
			&ref);
	}

	clang_disposeString(usr);
	clang_disposeString(fname);
	clang_disposeString(type_spelling);
	clang_disposeString(kind_spelling);
	clang_disposeString(spelling);
	return CXChildVisit_Recurse;
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

	if (argc < 3)
		usage(argc, argv);

	cxindex = clang_createIndex(0, 0);
	tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 2, argc - 2, // Skip over dbFilename
			0, 0,
			CXTranslationUnit_None);
	n = clang_getNumDiagnostics(tu);
	db = new walker_db(argv[1], 0, 0);
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
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		&visitor,
		db);
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
out:
	delete db;
	return 0;
}
