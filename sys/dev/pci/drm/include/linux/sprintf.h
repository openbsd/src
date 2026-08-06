/* Public domain. */

#ifndef _LINUX_SPRINTF_H
#define _LINUX_SPRINTF_H

#include <linux/types.h>

#define scnprintf(str, size, fmt, arg...) snprintf(str, size, fmt, ## arg)

static inline char *
kvasprintf(int flags, const char *fmt, va_list ap)
{
	char *buf;
	size_t len;
	va_list vl;

	va_copy(vl, ap);
	len = vsnprintf(NULL, 0, fmt, vl);
	va_end(vl);

	buf = malloc(len + 1, M_DRM, flags);
	if (buf) {
		vsnprintf(buf, len + 1, fmt, ap);
	}

	return buf;
}

static inline char *
kasprintf(int flags, const char *fmt, ...)
{
	char *buf;
	va_list ap;

	va_start(ap, fmt);
	buf = kvasprintf(flags, fmt, ap);
	va_end(ap);

	return buf;
}

static inline int
vscnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	int nc;

	nc = vsnprintf(buf, size, fmt, ap);
	if (nc > (size - 1))
		return (size - 1);
	else
		return nc;
}

#endif
