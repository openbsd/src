#	$OpenBSD: Makefile.inc,v 1.4 2001/09/10 22:38:11 millert Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/29 05:09:14 cgd Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c modf.c \
	setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	sigsetjmp.S
