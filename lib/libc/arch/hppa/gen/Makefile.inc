#	$OpenBSD: Makefile.inc,v 1.5 2002/05/17 18:41:02 mickey Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
