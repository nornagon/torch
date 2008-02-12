#include "assert.h"

#ifdef __cplusplus
extern "C" {
#endif

void _assert(const char *file, int line, const char *func, const char *expr, bool v) {
	if (!v) {
		printf("%s:%d in %s:\nassert(%s)", file, line, func, expr);
		while (1) swiWaitForVBlank();
	}
}

#ifdef __cplusplus
}
#endif
