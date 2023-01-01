/* Public domain. */

#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

#include <linux/types.h>

struct resource {
	u_long	start;
	u_long	end;
};

static inline resource_size_t
resource_size(const struct resource *r)
{
	return r->end - r->start + 1;
}

#define DEFINE_RES_MEM(_start, _size)		\
	{					\
		.start = (_start),		\
		.end = (_start) + (_size) - 1,	\
	}

#endif
