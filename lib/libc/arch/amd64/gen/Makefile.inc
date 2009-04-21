#	$OpenBSD: Makefile.inc,v 1.8 2009/04/21 09:34:50 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.S nan.c setjmp.S \
	sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

SRCS+=	alloca.S
LSRCS+= alloca.c
