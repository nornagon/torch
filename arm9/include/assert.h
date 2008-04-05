#ifndef ASSERT_H
#define ASSERT_H 1

#include <stdio.h>
#include <nds/jtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define assert(k) _assert(__FILE__,__LINE__,__func__,#k,(bool)(k))

void _assert(const char *, int, const char *, const char *, bool cond);

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H */
