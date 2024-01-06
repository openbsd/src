/* Public domain. */

#ifndef _LINUX_ALIGN_H
#define _LINUX_ALIGN_H

#include <sys/param.h>

#undef ALIGN
#define ALIGN(x, y) roundup2((x), (y))

#endif
