#	$Id: Makefile,v 1.6 2003/04/26 02:46:14 deraadt Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "mac68k"
PROG=	pdisk

SRCS=	bitfield.c convert.c deblock_media.c dump.c errors.c \
	file_media.c io.c media.c partition_map.c pathname.c \
	pdisk.c util.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc mac68k

.include <bsd.prog.mk>

