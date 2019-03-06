#	$OpenBSD: Makefile,v 1.7 2019/03/06 18:37:22 deraadt Exp $

PROG=	rsync
SRCS=	blocks.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c mkpath.c mktemp.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lcrypto -lm
DPADD+= ${LIBCRYPTO} ${LIBM}
xMAN=	rsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
