# $OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:11:27 tholo Exp $

SRCS+=	_setjmp.S alloca.S \
	fabs.c flt_rounds.c fpgetround.c fpsetround.c \
	fpgetmask.c fpsetmask.c fpgetsticky.c fpsetsticky.c \
	infinity.c isinf.c frexp.c ldexp.c modf.c \
	setjmp.S sigsetjmp.S
