#	$Id: Makefile,v 1.2 2001/04/17 05:09:22 drahn Exp $

.if ${MACHINE} == "powerpc
PROG=	pdisk

SRCS=	pdisk.c dump.c partition_map.c convert.c io.c errors.c bitfield.c \
	deblock_media.c file_media.c media.c pathname.c util.c validate.c
.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=powerpc

.include <bsd.prog.mk>
