/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: flt_rounds.c,v 1.2 1996/08/19 08:17:28 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	3,	/* round to negative infinity */
	2	/* round to positive infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return map[(x >> 30) & 0x03];
}
