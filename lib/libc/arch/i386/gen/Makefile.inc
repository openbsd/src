#	$OpenBSD: Makefile.inc,v 1.6 2008/07/24 09:31:06 martynas Exp $

SRCS+=	_setjmp.S alloca.S fabs.S infinity.c ldexp.c \
	modf.S nan.c setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S

LSRCS+= alloca.c
