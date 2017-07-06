# $OpenBSD: Makefile.inc,v 1.2 2017/07/06 13:11:15 bluhm Exp $

EXPATDIR=	${.CURDIR}/../../../../lib/libexpat
.PATH:		${EXPATDIR}/tests ${EXPATDIR}/tests/benchmark
PROG=		${.CURDIR:T}
CFLAGS=		-I${EXPATDIR}/lib
CPPFLAGS=	-I${EXPATDIR}/lib
LDADD=		-lexpat
DPADD=		${LIBEXPAT}
DEBUG?=		-g
