/*	$OpenBSD: pause.c,v 1.3 2001/09/04 23:35:58 millert Exp $	*/

/*
 * Written by Todd C. Miller <Todd.Miller@courtesan.com>
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: pause.c,v 1.3 2001/09/04 23:35:58 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <signal.h>

/*
 * Backwards compatible pause(3).
 */
int
pause()
{
	sigset_t mask;

	return (sigprocmask(SIG_BLOCK, NULL, &mask) ? -1 : sigsuspend(&mask));
}
