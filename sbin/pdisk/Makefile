#	$Id: Makefile,v 1.5 2002/04/20 22:51:32 miod Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "mac68k"
PROG=	pdisk

SRCS=	pdisk.c dump.c partition_map.c convert.c io.c errors.c bitfield.c \
	deblock_media.c file_media.c media.c pathname.c util.c validate.c
.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc mac68k

.include <bsd.prog.mk>
