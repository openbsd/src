#	$OpenBSD: Makefile.inc,v 1.3 2005/11/29 21:38:08 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.c isnan.c ldexp.c \
	modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

SRCS+=	alloca.S
LSRCS+= alloca.c
