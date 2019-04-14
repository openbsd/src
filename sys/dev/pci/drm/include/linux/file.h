/* Public domain. */

#ifndef _LINUX_FILE_H
#define _LINUX_FILE_H

/* both for printf */
#include <sys/types.h> 
#include <sys/systm.h>

#define fput(a)
#define fd_install(a, b)
#define put_unused_fd(a)

static inline int
get_unused_fd_flags(unsigned int flags)
{
	printf("%s: stub\n", __func__);
	return -1;
}

#endif
