#	$OpenBSD: Makefile.inc,v 1.1 2004/01/28 01:44:44 mickey Exp $

SRCS+=	_setjmp.S alloca.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c \
	modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

