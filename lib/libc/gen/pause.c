/*	$OpenBSD: pause.c,v 1.4 2003/06/25 21:15:04 deraadt Exp $	*/

/*
 * Written by Todd C. Miller <Todd.Miller@courtesan.com>
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: pause.c,v 1.4 2003/06/25 21:15:04 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <signal.h>
#include <unistd.h>

/*
 * Backwards compatible pause(3).
 */
int
pause()
{
	sigset_t mask;

	return (sigprocmask(SIG_BLOCK, NULL, &mask) ? -1 : sigsuspend(&mask));
}
