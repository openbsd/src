/*	$OpenBSD: defs.h,v 1.1.1.1 2002/02/10 22:51:41 fgsch Exp $	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#define WEAK_REF	0
#define STRONG_REF	1

int 	weak_func(void);
int	func(void);
