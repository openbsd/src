/*	$OpenBSD: ksh_limval.h,v 1.2 2004/12/18 20:55:52 millert Exp $	*/

/* Wrapper around the values.h/limits.h includes/ifdefs */

/* limits.h is included in sh.h */

#ifndef DMAXEXP
# define DMAXEXP	128	/* should be big enough */
#endif

#ifndef BITS
# define BITS(t)	(CHAR_BIT * sizeof(t))
#endif
