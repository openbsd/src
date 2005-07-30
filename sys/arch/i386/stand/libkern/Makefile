#	$OpenBSD: Makefile,v 1.1 2005/07/30 14:32:46 millert Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	kern

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..

.PATH:	${S}/lib/libkern

NOPROFILE=noprofile
NOPIC=nopic

# kern routines
SRCS+=	moddi3.c qdivrem.c strlcpy.c strlcat.c

all: ${KERNLIB}

install:

.include <bsd.lib.mk>
