#	$OpenBSD: Makefile.inc,v 1.12 2011/07/08 19:21:41 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	infinity.c ldexp.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c

SRCS+=	alloca.c
