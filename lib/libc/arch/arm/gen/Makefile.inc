# $OpenBSD: Makefile.inc,v 1.2 2004/02/01 05:40:52 drahn Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	alloca.S byte_swap_2.S byte_swap_4.S divsi3.S \
	fabs.c flt_rounds.c \
	infinity.c

# Common ieee754 constants and functions
#SRCS+=	nanf_ieee754.c	# infinity is ``different'' on arm, use local version
#SRCS+=	frexp_ieee754.c isinf_ieee754.c isinfl_ieee754.c isnan_ieee754.c
#SRCS+=	isnanl_ieee754.c ldexp_ieee754.c modf_ieee754.c

SRCS+=	setjmp.S
#SRCS+=	__setjmp14.S
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S
#SRCS+=	__sigsetjmp14.S

#SRCS+=	makecontext.c resumecontext.c swapcontext.S

#SRCS+=	_lwp.c

SRCS.arm.gen=	Lint_bswap16.c Lint_bswap32.c Lint_swapcontext.c
LSRCS+=		${SRCS.arm.gen}
DPSRCS+=	${SRCS.arm.gen}
CLEANFILES+=	${SRCS.arm.gen}

SRCS+= isinf.c isnan.c
SRCS+= fpgetround.S

SRCS+= modf_ieee754.c
SRCS+= ldexp.c
