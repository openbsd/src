#	$OpenBSD: Makefile,v 1.5 2019/02/13 05:41:35 tb Exp $

PROG=	rsync
SRCS=	blocks.c child.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c mkpath.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lcrypto -lm
DPADD+= ${LIBCRYPTO} ${LIBM}
xMAN=	rsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
