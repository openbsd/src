#	$Id: Makefile,v 1.7 2006/04/27 19:52:21 martin Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "mac68k"
PROG=	pdisk

CFLAGS+=-Wall -D__unix__

SRCS=	bitfield.c convert.c deblock_media.c dump.c errors.c \
	file_media.c hfs_misc.c io.c media.c partition_map.c pathname.c \
	pdisk.c util.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc mac68k

.include <bsd.prog.mk>

