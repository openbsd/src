#	$OpenBSD: Makefile.inc,v 1.1 2005/04/01 10:54:27 mickey Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
SRCS+=	alloca.c
