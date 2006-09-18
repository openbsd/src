#	$OpenBSD: Makefile,v 1.2 2006/09/18 21:14:15 mpf Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	kern

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..

.PATH:	${S}/lib/libkern

NOPROFILE=noprofile
NOPIC=nopic

# kern routines
SRCS+=	divdi3.c moddi3.c qdivrem.c strlcpy.c strlcat.c

all: ${KERNLIB}

install:

.include <bsd.lib.mk>
