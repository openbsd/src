#	$OpenBSD: Makefile,v 1.11 2021/08/29 13:43:46 claudio Exp $

PROG=	openrsync
SRCS=	blocks.c client.c downloader.c fargs.c flist.c hash.c ids.c \
	io.c log.c main.c misc.c mkpath.c mktemp.c receiver.c rmatch.c \
	rules.c sender.c server.c session.c socket.c symlinks.c uploader.c
LDADD+= -lcrypto -lm
DPADD+= ${LIBCRYPTO} ${LIBM}
MAN=	openrsync.1

CFLAGS+=-g -W -Wall -Wextra

openrsync.1: rsync.1
	ln -sf ${.CURDIR}/rsync.1 openrsync.1

CLEANFILES+=	openrsync.1

.include <bsd.prog.mk>
