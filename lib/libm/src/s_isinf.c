/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: s_isinf.c,v 1.3 1995/05/11 23:20:14 jtc Exp $";
#endif

/*
 * isinf(x) returns 1 is x is inf, else 0;
 * no branching!
 */

#include "math.h"
#include "math_private.h"

int
isinf(double x)
{
	int32_t hx,lx;
	EXTRACT_WORDS(hx,lx,x);
	hx &= 0x7fffffff;
	hx ^= 0x7ff00000;
	hx |= lx;
	return (hx == 0);
}
