/* Public domain. */

#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H

#define DECLARE_FLEX_ARRAY(t, n) \
	struct { struct{} n ## __unused; t n[]; }

#endif
