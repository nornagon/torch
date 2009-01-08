#include "assert.h"

#include <nds.h>
#ifdef NATIVE
#include "native.h"
#include <stdlib.h>
#else
#include "text.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void _assert(const char *file, int line, const char *func, const char *expr, bool v) {
	if (!v) {
		printf("%s:%d in %s:\nassert(%s)", file, line, func, expr);
#ifdef NATIVE
		printf("\n");
		abort();
#else
		text_console_enable();
		while (1) swiWaitForVBlank();
#endif
	}
}

void _require(const char *file, int line, const char *func, const char *expr, bool v, const char *str) {
	if (!v) {
		printf("%s:%d in %s:\n  requirement (%s) failed:\n  %s", file, line, func, expr, str);
#ifdef NATIVE
		printf("\n");
		abort();
#else
		while (1) swiWaitForVBlank();
#endif
	}
}

#ifdef __cplusplus
}
#endif
