#	$OpenBSD: Makefile.inc,v 1.11 2012/04/12 16:14:09 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.S nan.c setjmp.S \
	sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

SRCS+=	alloca.S
