/*	$OpenBSD: longjmp.c,v 1.2 2002/01/04 13:33:17 art Exp $	*/
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
 * The rlimit is here in case we start spinning.
 */
int
main()
{
	struct rlimit rl;
	volatile int i, expect;

	rl.rlim_cur = 2;
	rl.rlim_max = 2;
	if (setrlimit(RLIMIT_CPU, &rl) < 0)
		err(1, "setrlimit");

	expect = 0;
	i = setjmp(buf);
	if (i == 0 && expect != 0)
		errx(1, "setjmp returns 0 on longjmp(.., 0)");
	if (expect == 0) {
		expect = -1;
		longjmp(buf, 0);
	}

	expect = 0;
	i = setjmp(buf);
	if (i != expect)
		errx(1, "bad return from setjmp %d/%d", expect, i);
	if (expect < 1000)
		longjmp(buf, expect += 2);

	return 0;
}
