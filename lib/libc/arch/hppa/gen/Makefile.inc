#	$OpenBSD: Makefile.inc,v 1.10 2009/04/19 16:42:05 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c
SRCS+=	infinity.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c

SRCS+=	alloca.c
