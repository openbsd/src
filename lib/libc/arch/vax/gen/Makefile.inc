#	$OpenBSD: Makefile.inc,v 1.7 2009/04/21 09:34:51 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S \
	setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
