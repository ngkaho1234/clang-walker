#include <stdint.h>

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <string>

std::string cursor_name_str(CXCursor cursor)
{
	std::string str;
	CXString name = clang_getCursorSpelling(cursor);
	const char *name_cstr = clang_getCString(name);
	if (name_cstr)
		str = name_cstr;

	clang_disposeString(name);
	return str;
}

std::string file_name_str(CXFile file)
{
	std::string str;
	CXString fname = clang_getFileName(file);
	const char *fname_cstr = clang_getCString(fname);
	if (fname_cstr)
		str = fname_cstr;

	clang_disposeString(fname);
	return str;
}

std::string cursor_usr_str(CXCursor cursor)
{
	std::string str;
	CXString usr = clang_getCursorUSR(cursor);
	const char *usr_cstr = clang_getCString(usr);
	if (usr_cstr)
		str = usr_cstr;

	clang_disposeString(usr);
	return str;
}

std::string cursor_type_str(CXCursor cursor)
{
	std::string str;
	CXString type = clang_getTypeKindSpelling(
				clang_getCursorType(cursor).kind);
	const char *type_cstr = clang_getCString(type);
	if (type_cstr)
		str = type_cstr;

	clang_disposeString(type);
	return str;
}

std::string cursor_kind_str(CXCursor cursor)
{
	std::string str;
	CXString kind = clang_getCursorKindSpelling(
				clang_getCursorKind(cursor));
	const char *kind_cstr = clang_getCString(kind);
	if (kind_cstr)
		str = kind_cstr;

	clang_disposeString(kind);
	return str;
}

std::string cursor_linkage_str(CXCursor cursor)
{
	std::string str;
	CXLinkageKind linkage = clang_getCursorLinkage(cursor);
	const char *linkage_str;

	//
	// This conversion table is taken from
	// https://github.com/sabottenda/libclang-sample
	//
	switch (linkage) {
		case CXLinkage_Invalid:        linkage_str = "Invalid"; break;
		case CXLinkage_NoLinkage:      linkage_str = "NoLinkage"; break;
		case CXLinkage_Internal:       linkage_str = "Internal"; break;
		case CXLinkage_UniqueExternal: linkage_str = "UniqueExternal"; break;
		case CXLinkage_External:       linkage_str = "External"; break;
		default:                       linkage_str = "Unknown"; break;
	}
	str = linkage_str;
	return str;
}

enum CXChildVisitResult visitor(
		CXCursor cursor,
		CXCursor parent,
		CXClientData data)
{
	walker_db *db = (walker_db *)data;
	CXCursor referenced = clang_getCursorReferenced(cursor);
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXFile file;

	std::string fname;
	std::string usr_str = cursor_usr_str(referenced);
	std::string name_str = cursor_name_str(cursor);
	std::string type_str = cursor_type_str(cursor);
	std::string kind_str = cursor_kind_str(cursor);
	CXSourceLocation location = clang_getCursorLocation(cursor);
	unsigned int line, column, offs;
	uint32_t prop = 0;
	walker_ref ref;

	clang_getSpellingLocation(location, &file, &line, &column, &offs);
	fname = file_name_str(file);

	printf("Referenced USR: %s, cursor_kind: %s, "
		"cursor_linkage: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, column: %d, offs: %d, "
		"fname: %s\n",
		usr_str.c_str(), kind_str.c_str(),
		cursor_linkage_str(cursor).c_str(),
		type_str.c_str(), name_str.c_str(),
		line, column, offs,
		fname.c_str());

	if (clang_isDeclaration(cursor_kind))
		prop = REF_PROP_DECL;

	if (clang_isCursorDefinition(cursor))
		prop |= REF_PROP_DEF;

	clang_getCursorReferenced(cursor);
	if ((prop & REF_PROP_DECL) && !usr_str.empty()) {
		ref.assign(
			prop,
			line, column, offs, &fname);
		db->set(usr_str.c_str(),
			&ref);
	}

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
	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		&visitor,
		db);
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
	delete db;
	return 0;
}
