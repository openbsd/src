# $OpenBSD: Makefile.inc,v 1.5 2004/02/02 16:09:45 drahn Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	alloca.S byte_swap_2.S byte_swap_4.S divsi3.S \
	fabs.c flt_rounds.c \
	infinity.c

SRCS+=	setjmp.S
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

SRCS+= isinf.c isnan.c

SRCS+= modf_ieee754.c
SRCS+= ldexp.c
SRCS+= frexp.c
