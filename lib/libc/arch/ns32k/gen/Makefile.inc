#	$OpenBSD: Makefile.inc,v 1.2 1996/04/21 23:38:37 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/04/05 00:23:05 phil Exp $

SRCS+=	__main.c _setjmp.S alloca.S fabs.S frexp.S infinity.c isinf.c
SRCS+=	ldexp.S modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
