#include <clang-c/Index.h>

#include <stdio.h>

int main(int argc, char **argv)
{
	CXIndex cxindex = clang_createIndex(0, 0);
	CXTranslationUnit tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 1, argc - 1, // Skip over dbFilename and indexFilename
			0, 0,
			CXTranslationUnit_None);
	int n = clang_getNumDiagnostics(tu);

	if (n > 0) {
		int nErrors = 0;
		int i;
		for (i = 0; i < n; ++i) {
			CXDiagnostic diag = clang_getDiagnostic(tu, i);
			CXString string = clang_formatDiagnostic(diag,
						clang_defaultDiagnosticDisplayOptions());
			fprintf(stderr, "%s\n", clang_getCString(string));
			if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Error
					|| clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal)
				nErrors++;
		}
	}
	return 0;
}
