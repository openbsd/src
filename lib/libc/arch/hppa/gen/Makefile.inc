#	$OpenBSD: Makefile.inc,v 1.6 2003/05/02 19:20:26 millert Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
SRCS+=	alloca.c
