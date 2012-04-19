#	$OpenBSD: Makefile.inc,v 1.14 2012/04/19 19:14:56 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c \
	modf.S nan.c setjmp.S sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S
