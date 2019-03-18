#	$OpenBSD: Makefile,v 1.8 2019/03/18 04:32:31 deraadt Exp $

PROG=	openrsync
SRCS=	blocks.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c mkpath.c mktemp.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lcrypto -lm
DPADD+= ${LIBCRYPTO} ${LIBM}
MAN=	openrsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

openrsync.1: rsync.1
	ln -sf ${.CURDIR}/rsync.1 openrsync.1

CLEANFILES+=	openrsync.1

.include <bsd.prog.mk>
