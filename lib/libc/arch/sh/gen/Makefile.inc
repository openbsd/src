#	$OpenBSD: Makefile.inc,v 1.7 2009/04/19 16:42:06 martynas Exp $

SRCS+= flt_rounds.c infinity.c nan.c setjmp.S _setjmp.S sigsetjmp.S
SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c
