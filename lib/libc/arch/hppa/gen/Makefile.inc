#	$OpenBSD: Makefile.inc,v 1.7 2005/02/01 15:12:29 millert Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
SRCS+=	alloca.c
