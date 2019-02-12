#	$OpenBSD: Makefile,v 1.4 2019/02/12 19:04:52 benno Exp $

PROG=	rsync
SRCS=	blocks.c child.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c md4.c mkpath.c receiver.c sender.c server.c session.c \
	socket.c symlinks.c uploader.c main.c
LDADD+= -lm
DPADD+= ${LIBM}
xMAN=	rsync.1

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
