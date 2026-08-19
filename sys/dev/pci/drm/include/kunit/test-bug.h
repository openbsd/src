/* Public domain. */

#ifndef _KUNIT_TEST_BUG_H
#define _KUNIT_TEST_BUG_H

#include <sys/param.h>

#define kunit_fail_current_test(fmt, ...)

static inline void *
kunit_get_current_test(void)
{
	return NULL;
}

#endif
