#	$OpenBSD: Makefile.inc,v 1.9 2011/07/08 22:28:33 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S \
	setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
