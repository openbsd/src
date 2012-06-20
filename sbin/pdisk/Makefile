#	$Id: Makefile,v 1.10 2012/06/20 18:33:59 matthew Exp $

.if ${MACHINE} == "macppc" || ${MACHINE} == "mac68k"
PROG=	pdisk
LDADD=  -lutil
DPADD=  ${LIBUTIL}
CFLAGS+=-Wall

SRCS=	bitfield.c convert.c deblock_media.c dump.c errors.c \
	file_media.c hfs_misc.c io.c media.c partition_map.c pathname.c \
	pdisk.c util.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc

.include <bsd.prog.mk>

