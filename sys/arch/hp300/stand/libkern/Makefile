#	$OpenBSD: Makefile,v 1.3 1997/07/14 08:14:44 downsj Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern

CFLAGS= -O2 -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

# kern routines
SRCS=	__main.c ashrdi3.c divdi3.c qdivrem.c strlen.c strcat.c strcmp.c \
	strcpy.c strncmp.c umoddi3.c

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
