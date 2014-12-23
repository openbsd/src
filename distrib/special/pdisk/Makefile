#	$Id: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

.if ${MACHINE} == "macppc"
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

.PATH:  ${.CURDIR}/../../../sbin/pdisk
.include <bsd.prog.mk>
