/*

includes.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Thu Mar 23 16:29:37 1995 ylo

This file includes most of the needed system headers.

*/

/* RCSID("$Id: includes.h,v 1.5 1999/09/30 05:03:04 deraadt Exp $"); */

#ifndef INCLUDES_H
#define INCLUDES_H

/* Note: autoconf documentation tells to use the <...> syntax and have -I. */
#include <config.h>

#include "version.h"

typedef unsigned short word16;

#if SIZEOF_LONG == 4
typedef unsigned long word32;
#else
#if SIZEOF_INT == 4
typedef unsigned int word32;
#else
#if SIZEOF_SHORT >= 4
typedef unsigned short word32;
#else
YOU_LOSE
#endif
#endif
#endif

#include <sys/param.h>
#include <machine/endian.h>
#include <netgroup.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#include <sys/ioctl.h>

#include <termios.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#include <pwd.h>
#include <grp.h>

#include <sys/wait.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <sys/time.h>
#include <time.h>

#include <paths.h>

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#include <sys/resource.h>

#if USE_STRLEN_FOR_AF_UNIX
#define AF_UNIX_SIZE(unaddr) \
  (sizeof((unaddr).sun_family) + strlen((unaddr).sun_path) + 1)
#else
#define AF_UNIX_SIZE(unaddr) sizeof(unaddr)
#endif

#endif /* INCLUDES_H */
