#	$NetBSD: Makefile.inc,v 1.3 1995/04/11 18:13:49 jtc Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.S ldexp.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S
