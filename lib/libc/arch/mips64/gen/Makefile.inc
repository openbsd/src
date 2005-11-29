#	$OpenBSD: Makefile.inc,v 1.3 2005/11/29 21:38:09 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.S ldexp.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S

SRCS+=	alloca.c
