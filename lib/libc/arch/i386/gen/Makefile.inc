#	$OpenBSD: Makefile.inc,v 1.11 2011/07/08 19:21:41 martynas Exp $

SRCS+=	_setjmp.S alloca.S infinity.c ldexp.c \
	nan.c setjmp.S sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S

LSRCS+= alloca.c
