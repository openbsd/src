/* Public domain. */

#ifndef _LINUX_ERRNO_H
#define _LINUX_ERRNO_H

#include <sys/errno.h>

#define ERESTARTSYS	EINTR
#define ETIME		ETIMEDOUT
#define EREMOTEIO	EIO
#define ENOTSUPP	ENOTSUP
#define ENODATA		ENOTSUP
#define ECHRNG		EINVAL

#endif
