#	$OpenBSD: Makefile.inc,v 1.6 2008/07/24 09:31:06 martynas Exp $

SRCS+= flt_rounds.c infinity.c nan.c setjmp.S _setjmp.S sigsetjmp.S \
	modf.c ldexp.c

SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c
