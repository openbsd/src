/* System-dependent stuff, for Linux systems.  Known to be good for
   Linux/Alpha, but should work for all other platforms, too. */

/*
 * This is important on Linux/Alpha where sizeof(void*) != sizeof(int).
 */
#define HAVE_VARARGS_H

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
#include <string.h>

#include <dirent.h>
typedef struct dirent dirent;
