/* $OpenBSD: sysdep-openbsd.h,v 1.1 1998/05/17 19:43:25 marc Exp $

/* System defines for OpenBSD.
   If you think you need to change this file, then you are wrong.  In order to
   avoid a huge ugly mass of nested #ifdefs, you should create a new file just
   for your system, which contains exactly those #includes and definitions that
   your system needs, AND NOTHING MORE!  Then, add that file to the appropriate
   place in configure.in, and viola, you are done.  sysdep-sunos4.h is a good
   example of how to do this. */

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#ifndef alloca				/* May be a macro, with args. */
extern char *alloca ();
#endif
#endif

#include <sys/types.h>			/* Needed by dirent.h */
#include <dirent.h>
typedef struct dirent dirent;
#include <unistd.h>			/* for _POSIX_VERSION */
