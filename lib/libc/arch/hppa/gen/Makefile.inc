#	$OpenBSD: Makefile.inc,v 1.4 2002/03/11 02:59:01 miod Exp $

#SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	isnan.c isinf.c infinity.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	modf.c
