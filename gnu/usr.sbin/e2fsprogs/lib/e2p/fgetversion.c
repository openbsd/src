/*
 * fgetversion.c	- Get a file version on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>

#include "e2p.h"

int fgetversion (const char * name, unsigned long * version)
{
#if HAVE_EXT2_IOCTLS
	int fd;
	int r;

	fd = open (name, O_RDONLY|O_NONBLOCK);
	if (fd == -1)
		return - 1;
	r = ioctl (fd, EXT2_IOC_GETVERSION, version);
	close (fd);
	return r;
#else /* ! HAVE_EXT2_IOCTLS */
	extern int errno;
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
}
