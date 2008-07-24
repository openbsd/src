#	$OpenBSD: Makefile.inc,v 1.6 2008/07/24 09:31:07 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S \
	modf.S setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
