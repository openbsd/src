#	$OpenBSD: Makefile,v 1.3 2000/09/03 21:23:42 mickey Exp $

LIB=	kern

S=	${.CURDIR}/../../../..
M=	${KERNDIR}/arch/${MACHINE_ARCH}
SADIR=	${.CURDIR}/..
KERNDIR=$S/lib/libkern

NOPIC=		nopic
NOPROFILE=	noprofile

.PATH:	${KERNDIR}

.include "arch/hppa/Makefile.inc"

# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c

# Other stuff
SRCS+=	srandom.c bcd.c

OBJS+=	bzero.o

install:

.include <bsd.lib.mk>
CPPFLAGS+=	${DEBUGFLAGS}
CFLAGS+=	${SACFLAGS}
