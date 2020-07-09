/* Public domain. */

#ifndef _LINUX_OVERFLOW_H
#define _LINUX_OVERFLOW_H

#define array_size(x, y)	((x) * (y))

#define struct_size(p, member, n) \
	(sizeof(*(p)) + ((n) * (sizeof(*(p)->member))))

#endif
