/* Public domain. */

#ifndef _LINUX_KCONFIG_H
#define _LINUX_KCONFIG_H

#include <sys/endian.h>

#include <generated/autoconf.h>

#define IS_ENABLED(x) x - 0
#define IS_BUILTIN(x) 1

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

#endif
