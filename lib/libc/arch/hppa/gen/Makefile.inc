#	$OpenBSD: Makefile.inc,v 1.8 2005/11/29 21:38:08 deraadt Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c

SRCS+=	alloca.c
