#ifndef ASSERT_H
#define ASSERT_H 1

#include <nds.h>
#include <stdio.h>

#define assert(k) _assert(__FILE__,__LINE__,__func__,#k,(bool)(k))

void _assert(const char *file, int line, const char *func, const char *expr, bool v) {
	if (!v) {
		printf("%s:%d in %s:\nassert(%s)", file, line, func, expr);
		while (1) swiWaitForVBlank();
	}
}

#endif /* ASSERT_H */
