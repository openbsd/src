#	$OpenBSD: Makefile,v 1.6 2019/02/16 10:46:22 florian Exp $

PROG=	rsync
SRCS=	blocks.c child.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c mkpath.c mktemp.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lcrypto -lm
DPADD+= ${LIBCRYPTO} ${LIBM}
xMAN=	rsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
