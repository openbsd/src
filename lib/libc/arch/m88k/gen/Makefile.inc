#	$OpenBSD: Makefile.inc,v 1.16 2013/06/05 22:06:30 miod Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/10 21:09:06 jtc Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.c nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	divsi3.S udivsi3.S modsi3.S umodsi3.S
