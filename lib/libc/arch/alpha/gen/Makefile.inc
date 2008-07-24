#	$OpenBSD: Makefile.inc,v 1.8 2008/07/24 09:31:06 martynas Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/29 05:09:14 cgd Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	sigsetjmp.S

SRCS+=	alloca.c
