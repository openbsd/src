#	$OpenBSD: Makefile,v 1.1 1998/06/23 19:47:28 mickey Exp $

LIB=	kern

S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..
DIR_KERN=$S/lib/libkern

NOPIC=		nopic
NOPROFILE=	noprofile

.PATH:	${DIR_KERN}

.include "arch/hppa/Makefile.inc"

# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c

# Other stuff
SRCS+=	srandom.c bcd.c

install:

.include <bsd.lib.mk>
CPPFLAGS+=	${DEBUGFLAGS}
CFLAGS+=	${SACFLAGS}
