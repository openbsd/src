/*
 * setflags.c		- Set a file flags on an ext2 file system
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
#if HAVE_CHFLAGS
#include <sys/types.h>
#include <sys/stat.h>		/* For the flag values.  */
#else
#include <sys/ioctl.h>
#endif

#include "e2p.h"

int setflags (int fd, unsigned long flags)
{
#if HAVE_CHFLAGS
  unsigned long bsd_flags = 0;

#ifdef UF_IMMUTABLE
  if (flags & EXT2_IMMUTABLE_FL)
    bsd_flags |= UF_IMMUTABLE;
#endif
#ifdef UF_APPEND
  if (flags & EXT2_APPEND_FL)
    bsd_flags |= UF_APPEND;
#endif
#ifdef UF_NODUMP
  if (flags & EXT2_NODUMP_FL)
    bsd_flags |= UF_NODUMP;
#endif

  return fchflags (fd, bsd_flags);
#else
#if HAVE_EXT2_IOCTLS
	return ioctl (fd, EXT2_IOC_SETFLAGS, &flags);
#else /* ! HAVE_EXT2_IOCTLS */
	extern int errno;
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
#endif
}
