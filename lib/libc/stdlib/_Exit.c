/*	$OpenBSD: _Exit.c,v 1.1 2004/05/03 17:21:13 millert Exp $	*/

/*
 * Placed in the public domain by Todd C. Miller on January 21, 2004.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: _Exit.c,v 1.1 2004/05/03 17:21:13 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <stdlib.h>
#include <unistd.h>

/*
 * _Exit() is the ISO/ANSI C99 equivalent of the POSIX _exit() function.
 * No atexit() handlers are called and no signal handlers are run.
 * Whether or not stdio buffers are flushed or temporary files are removed
 * is implementation-dependent.  As such it is safest to *not* flush
 * stdio buffers or remove temporary files.  This is also consistent
 * with most other implementations.
 */
void
_Exit(int status)
{
	_exit(status);
}
