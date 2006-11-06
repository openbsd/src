#	$OpenBSD: Makefile.inc,v 1.4 2006/11/06 15:14:50 drahn Exp $

SRCS+= flt_rounds.c isinf.c isnan.c infinity.c setjmp.S _setjmp.S sigsetjmp.S \
	modf.c ldexp.c

#hardfloat only pieces
#SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
#	fpsetmask.c fpsetround.c fpsetsticky.c \
