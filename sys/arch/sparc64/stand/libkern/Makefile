#	$OpenBSD: Makefile,v 1.1.1.1 2001/08/18 15:51:10 jason Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern
.PATH:	${.CURDIR}/../../../../lib/libkern/arch/sparc

.include "${.CURDIR}/../../../../lib/libkern/arch/sparc/Makefile.inc"

CFLAGS= -O2 -nostdinc -I${.CURDIR}/../include -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libkern \
	-I${.CURDIR}/../../../../lib/libkern/arch/sparc -I${.CURDIR}

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern \
	-I${.CURDIR}/../../../../lib/libkern/arch/sparc

.NOPATH: machine

# kern routines
SRCS=	__main.c ashrdi3.c bzero.S divdi3.c qdivrem.c strlen.S strcat.c \
	strcmp.c strcpy.c strncmp.c umoddi3.c

SRCS+=	umul.S mul.S rem.S sdiv.S udiv.S urem.S
SRCS+=	saveregs.S

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>

machine:
	rm -f $@
	ln -s ${.CURDIR}/../../include $@
