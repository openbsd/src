#	$NetBSD: Makefile.inc,v 1.4 1995/04/09 20:33:13 mycroft Exp $

SRCS+=	_setjmp.S alloca.S fabs.S frexp.c infinity.c isinf.c ldexp.c modf.S \
	setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S
