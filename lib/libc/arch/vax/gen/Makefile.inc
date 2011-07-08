#	$OpenBSD: Makefile.inc,v 1.8 2011/07/08 19:21:42 martynas Exp $

SRCS+=	_setjmp.S infinity.c ldexp.S \
	setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
