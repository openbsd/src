#	$OpenBSD: Makefile.inc,v 1.5 2005/11/29 21:38:10 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.c ldexp.S \
	modf.S setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
LSRCS+= alloca.c
