/*	$OpenBSD: longjmp.c,v 1.1 2002/01/04 13:02:57 art Exp $	*/
/*
 * Artur Grabowski <art@openbsd.org> Public Domain.
 */

#include <setjmp.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>


jmp_buf buf;

/*
 * When longjmp is passed the incorrect arg (0), it should translate it into
 * something better.
 *
 * Test is simple. rlimit the cpu time and throw an incorrect longjmp. If 0
 * is not translated we'll spin until we hit the cpu time limit.
 */
int
main()
{
	struct rlimit rl;

	rl.rlim_cur = 2;
	rl.rlim_max = 2;
	if (setrlimit(RLIMIT_CPU, &rl) < 0)
		err(1, "setrlimit");

	if (setjmp(buf) == 0)
		longjmp(buf, 0);

	return 0;
}
