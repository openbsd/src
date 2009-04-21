#	$OpenBSD: Makefile.inc,v 1.10 2009/04/21 09:34:50 martynas Exp $

SRCS+=	_setjmp.S alloca.S fabs.S infinity.c ldexp.c \
	modf.S nan.c setjmp.S sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S

LSRCS+= alloca.c
