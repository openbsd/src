#	$OpenBSD: Makefile,v 1.5 2012/08/21 14:46:18 pascal Exp $

CFLAGS+=-D__INTERNAL_LIBSA_CREAD

NOPIE=

# io routines
SRCS+=	cread.c

.include "${.CURDIR}/../libsa/Makefile"
.PATH:	${.CURDIR}/../libsa

LIB=	saz
