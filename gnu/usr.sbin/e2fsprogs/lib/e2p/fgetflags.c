/*
 * fgetflags.c		- Get a file flags on an ext2 file system
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
#if HAVE_STAT_FLAGS
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

#include "e2p.h"

int fgetflags (const char * name, unsigned long * flags)
{
#if HAVE_STAT_FLAGS
  struct stat buf;

  if (stat (name, &buf) == -1)
    return -1;

  *flags = 0;
#ifdef UF_IMMUTABLE
  if (buf.st_flags & UF_IMMUTABLE)
    *flags |= EXT2_IMMUTABLE_FL;
#endif
#ifdef UF_APPEND
  if (buf.st_flags & UF_APPEND)
    *flags |= EXT2_APPEND_FL;
#endif
#ifdef UF_NODUMP
  if (buf.st_flags & UF_NODUMP)
    *flags |= EXT2_NODUMP_FL;
#endif

  return 0;
#else
#if HAVE_EXT2_IOCTLS
	int fd;
	int r;

	fd = open (name, O_RDONLY|O_NONBLOCK);
	if (fd == -1)
		return -1;
	r = ioctl (fd, EXT2_IOC_GETFLAGS, flags);

	close (fd);
	return r;
#else /* ! HAVE_EXT2_IOCTLS */
	extern int errno;
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
#endif
}
