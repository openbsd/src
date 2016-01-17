#	$Id: Makefile,v 1.20 2016/01/17 14:13:42 jasper Exp $

.if ${MACHINE} == "amd64"
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

.include <bsd.prog.mk>
