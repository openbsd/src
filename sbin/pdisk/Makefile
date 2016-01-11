#	$Id: Makefile,v 1.14 2016/01/11 23:31:27 krw Exp $

.if ${MACHINE} == "macppc"
PROG=	pdisk
LDADD=  -lutil
DPADD=  ${LIBUTIL}
CFLAGS+=-Wall

SRCS=	bitfield.c convert.c deblock_media.c dump.c errors.c \
	file_media.c hfs_misc.c io.c media.c partition_map.c \
	pdisk.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc

.include <bsd.prog.mk>
