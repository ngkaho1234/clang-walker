#ifndef _CLANG_WALKER_HXX_
#define _CLANG_WALKER_HXX_

#include "clang-walker-db.hxx"

#include <clang-c/Index.h>

struct walker {
	int (*symbol_op)(
			const char *usr,
			walker_ref *ref,
			void *args);
	void *args;

	// Any non-zero return values indicate error
	int retval;
};

enum CXChildVisitResult walker_visitor(
		CXCursor cursor,
		CXCursor parent,
		CXClientData data);

#endif // _CLANG_WALKER_DB_HXX_
