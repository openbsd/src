#	$OpenBSD: Makefile.inc,v 1.10 2012/04/12 16:14:09 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S \
	setjmp.S udiv.S urem.S sigsetjmp.S

SRCS+= alloca.S
