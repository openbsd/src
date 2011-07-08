#	$OpenBSD: Makefile.inc,v 1.13 2011/07/08 22:28:33 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c
SRCS+=	infinity.c ldexp.c modf.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c

SRCS+=	alloca.c
