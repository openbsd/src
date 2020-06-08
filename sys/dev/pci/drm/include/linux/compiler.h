/* Public domain. */

#ifndef _LINUX_COMPILER_H
#define _LINUX_COMPILER_H

#include <linux/kconfig.h>
#include <sys/atomic.h>		/* for READ_ONCE() WRITE_ONCE() */

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define __force
#define __acquires(x)
#define __releases(x)
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
#define fallthrough	do {} while (0)

#ifndef __user
#define __user
#endif

#define barrier()	__asm __volatile("" : : : "memory")

#define __printf(x, y)

#define uninitialized_var(x) x

/* The Linux code doesn't meet our usual standards! */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wenum-conversion"
#pragma clang diagnostic ignored "-Winitializer-overrides"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wparentheses-equality"
#pragma clang diagnostic ignored "-Wmissing-braces"
#else
#pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif

#endif
