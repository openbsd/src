#	$OpenBSD: Makefile.inc,v 1.5 2003/05/02 19:20:26 millert Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/29 05:09:14 cgd Exp $

SRCS+=	_setjmp.S alloca.c fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c \
	modf.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	sigsetjmp.S
