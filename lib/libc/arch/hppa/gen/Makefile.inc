#	$OpenBSD: Makefile.inc,v 1.9 2008/07/24 09:31:06 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c ldexp.c
SRCS+=	infinity.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c

SRCS+=	alloca.c
