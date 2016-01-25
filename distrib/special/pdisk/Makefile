#	$Id: Makefile,v 1.9 2016/01/25 23:43:20 krw Exp $

.if ${MACHINE} == "macppc"
PROG=	pdisk
LDADD=  -lutil
DPADD=  ${LIBUTIL}
CFLAGS+=-Wall

SRCS=	dump.c \
	file_media.c io.c partition_map.c \
	pdisk.c validate.c

.else
NOPROG=yes
.endif

MAN=	pdisk.8
MANSUBDIR=macppc

.PATH:  ${.CURDIR}/../../../sbin/pdisk
.include <bsd.prog.mk>
