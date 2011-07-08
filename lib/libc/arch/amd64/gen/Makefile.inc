#	$OpenBSD: Makefile.inc,v 1.9 2011/07/08 19:21:41 martynas Exp $

SRCS+=	_setjmp.S infinity.c ldexp.c nan.c setjmp.S \
	sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

SRCS+=	alloca.S
LSRCS+= alloca.c
