#	$OpenBSD: Makefile.inc,v 1.5 2007/03/02 06:11:54 miod Exp $

SRCS+= flt_rounds.c isinf.c isnan.c infinity.c setjmp.S _setjmp.S sigsetjmp.S \
	modf.c ldexp.c

SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c
