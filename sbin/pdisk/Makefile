#	$Id: Makefile,v 1.4 2001/09/01 16:21:16 drahn Exp $

.if ${MACHINE} == "macppc"
PROG=	pdisk

SRCS=	pdisk.c dump.c partition_map.c convert.c io.c errors.c bitfield.c \
	deblock_media.c file_media.c media.c pathname.c util.c validate.c
.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc

.include <bsd.prog.mk>
