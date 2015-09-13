/*	$OpenBSD: ksh_limval.h,v 1.3 2015/09/13 19:43:42 tedu Exp $	*/

/* Wrapper around the values.h/limits.h includes/ifdefs */

/* limits.h is included in sh.h */

#ifndef BITS
# define BITS(t)	(CHAR_BIT * sizeof(t))
#endif
