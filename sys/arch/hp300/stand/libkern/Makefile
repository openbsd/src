#	$OpenBSD: Makefile,v 1.2 1997/01/18 03:18:18 downsj Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern
.PATH:	${.CURDIR}/..

CFLAGS+-Dhp300 =${DEBUGFLAGS}
CFLAGS+=-I${.CURDIR}/..
CFLAGS+=-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

# kern routines
SRCS=	__main.c ashrdi3.c divdi3.c qdivrem.c strlen.c strcat.c strcmp.c \
	strcpy.c strncmp.c umoddi3.c

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
