/* Public domain. */

#ifndef _LINUX_PRINTK_H
#define _LINUX_PRINTK_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/stdarg.h>

#define KERN_INFO	""
#define KERN_WARNING	""
#define KERN_NOTICE	""
#define KERN_DEBUG	""
#define KERN_CRIT	""
#define KERN_ERR	""

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define printk_once(fmt, arg...) ({		\
	static int __warned;			\
	if (!__warned) {			\
		printf(fmt, ## arg);		\
		__warned = 1;			\
	}					\
})

#define printk(fmt, arg...)	printf(fmt, ## arg)
#define pr_warn(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_warn_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_notice(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_crit(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_err(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_cont(fmt, arg...)	printf(pr_fmt(fmt), ## arg)

#ifdef DRMDEBUG
#define pr_info(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_info_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_debug(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#else
#define pr_info(fmt, arg...)	do { } while(0)
#define pr_info_once(fmt, arg...)	do { } while(0)
#define pr_debug(fmt, arg...)	do { } while(0)
#endif

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

void print_hex_dump(const char *, const char *, int, int, int,
	 const void *, size_t, bool);

struct va_format {
	const char *fmt;
	va_list *va;
};

#endif
