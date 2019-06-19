/* Public domain. */

#ifndef _LINUX_COMPILER_H
#define _LINUX_COMPILER_H

#include <linux/kconfig.h>

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define __force
#define __always_unused __unused
#define __maybe_unused
#define __read_mostly
#define __iomem
#define __must_check
#define __init
#define __exit
#define __deprecated
#define __always_inline inline
#define noinline __attribute__((noinline))

#ifndef __user
#define __user
#endif

#define barrier()	__asm __volatile("" : : : "memory")

#define __printf(x, y)

#define uninitialized_var(x) x

#endif
