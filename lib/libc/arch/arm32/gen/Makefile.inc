# $NetBSD: Makefile.inc,v 1.2 1996/05/12 20:34:15 mark Exp $

SRCS+=	_setjmp.S alloca.S \
	fabs.c flt_rounds.c fpgetround.c fpsetround.c \
	fpgetmask.c fpsetmask.c fpgetsticky.c fpsetsticky.c \
	infinity.c isinf.c frexp.c ldexp.c modf.c \
	setjmp.S sigsetjmp.S
