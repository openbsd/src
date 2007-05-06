#	$OpenBSD: Makefile,v 1.2 2007/05/06 02:48:44 deraadt Exp $

LIB=	kern

S=	${.CURDIR}/../../../..
M=	${KERNDIR}/arch/${MACHINE_ARCH}
SADIR=	${.CURDIR}/..
KERNDIR=$S/lib/libkern

NOPIC=		nopic
NOPROFILE=	noprofile

.PATH:	${KERNDIR}

.include "arch/hppa64/Makefile.inc"

# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c

# Other stuff
SRCS+=	srandom.c

install:

.include <bsd.lib.mk>
CPPFLAGS+=	${DEBUGFLAGS}
CFLAGS+=	${SACFLAGS}
