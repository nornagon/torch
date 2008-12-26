#ifndef ASSERT_H
#define ASSERT_H 1

#include <stdio.h>
#include <nds/jtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined __cplusplus
# define __ASSERT_VOID_CAST static_cast<void>
#else
# define __ASSERT_VOID_CAST (void)
#endif

#ifdef NDEBUG
#define assert(k) (__ASSERT_VOID_CAST (0))
#else
#ifdef NATIVE
#define assert(k) _assert(__FILE__,__LINE__,__PRETTY_FUNCTION__,__STRING(k),(bool)(k))
#else
#define assert(k) _assert(__FILE__,__LINE__,__func__,#k,(bool)(k))
#endif
#endif

void _assert(const char *, int, const char *, const char *, bool cond);

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H */
