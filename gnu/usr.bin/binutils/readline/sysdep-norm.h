/* System-dependent stuff, for ``normal'' systems */
/* If you think you need to change this file, then you are wrong.  In order to
   avoid a huge ugly mass of nested #ifdefs, you should create a new file just
   for your system, which contains exactly those #includes and definitions that
   your system needs, AND NOTHING MORE!  Then, add that file to the appropriate
   place in configure.in, and viola, you are done.  sysdep-sunos4.h is a good
   example of how to do this. */

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#if defined (sparc) && defined (sun)
#include <alloca.h>
#endif
#ifndef alloca				/* May be a macro, with args. */
extern char *alloca ();
#endif
#endif

#include <sys/types.h>			/* Needed by dirent.h */

#if defined (USG) && defined (TIOCGWINSZ)
#include <sys/stream.h>
#if defined (USGr4) || defined (USGr3)
#include <sys/ptem.h>
#endif /* USGr4 */
#endif /* USG && TIOCGWINSZ */

#ifndef _WIN32
#include <dirent.h>
typedef struct dirent dirent;
#endif

/* SVR4 systems should use <termios.h> rather than <termio.h>. */

#if defined (USGr4)
#define _POSIX_VERSION
#endif

#if defined _WIN32 && !defined __GNUC__
#include <malloc.h>
#endif
