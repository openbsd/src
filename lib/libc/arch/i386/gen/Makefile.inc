#	$OpenBSD: Makefile.inc,v 1.3 2001/09/10 22:38:11 millert Exp $

SRCS+=	_setjmp.S alloca.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c \
	modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S
