#	$OpenBSD: Makefile.inc,v 1.9 2011/07/08 19:21:42 martynas Exp $

SRCS+= flt_rounds.c infinity.c ldexp.c nan.c setjmp.S _setjmp.S
SRCS+= sigsetjmp.S
SRCS+=	fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c
