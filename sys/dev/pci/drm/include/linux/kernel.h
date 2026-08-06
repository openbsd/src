/* Public domain. */

#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stdarg.h>
#include <sys/malloc.h>

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/linkage.h>
#include <linux/printk.h>
#include <linux/typecheck.h>
#include <linux/container_of.h>
#include <linux/stddef.h>
#include <linux/align.h>
#include <linux/math.h>
#include <linux/limits.h>
#include <asm/byteorder.h>
#include <linux/wordpart.h>
#include <linux/array_size.h>
#include <linux/sprintf.h>
#include <linux/minmax.h>
#include <linux/panic.h>

#define might_sleep()		assertwaitok()
#define might_sleep_if(x)	do {	\
	if (x)				\
		assertwaitok();		\
} while (0)
#define might_fault()

#define u64_to_user_ptr(x)	((void *)(uintptr_t)(x))

#define _RET_IP_		__builtin_return_address(0)
#define _THIS_IP_		0

#define STUB() do { printf("%s: stub\n", __func__); } while(0)

#define PTR_IF(c, p)		((c) ? (p) : NULL)

#endif
