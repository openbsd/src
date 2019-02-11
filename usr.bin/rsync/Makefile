#	$OpenBSD: Makefile,v 1.3 2019/02/11 16:36:34 deraadt Exp $

PROG=	rsync
SRCS=	blocks.c child.c client.c downloader.c fargs.c flist.c hash.c \
	io.c log.c md4.c mkpath.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lm
DPADD+= ${LIBM}
xMAN=	rsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
