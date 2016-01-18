#	$Id: Makefile,v 1.8 2016/01/18 03:10:16 deraadt Exp $

.if ${MACHINE} == "macppc"
PROG=	pdisk
LDADD=  -lutil
DPADD=  ${LIBUTIL}
CFLAGS+=-Wall

SRCS=	convert.c dump.c \
	file_media.c io.c partition_map.c \
	pdisk.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc

.PATH:  ${.CURDIR}/../../../sbin/pdisk
.include <bsd.prog.mk>
