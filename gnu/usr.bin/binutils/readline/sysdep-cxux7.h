/* System-dependent stuff, for Harris CX/UX systems */

#ifndef alloca				/* May be a macro, with args. */
extern char *alloca ();
#endif

#include <sys/types.h>			/* Needed by dirent.h */
#include <sys/stream.h>
#include <dirent.h>
typedef struct dirent dirent;

/* SVR4 systems should use <termios.h> rather than <termio.h>. */
#define _POSIX_VERSION

/* SVR4 systems need _POSIX_SOURCE defined to suppress 'struct winsize'
   definition in <termios.h>, since it's unconditionally defined in
   <sys/ptem.h>. */
#define _POSIX_SOURCE
