#	$OpenBSD: Makefile,v 1.5 2001/05/04 22:16:18 millert Exp $

PROG=	supscan
NOMAN=
SRCS=	supscan.c stree.c scan.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c skipto.c vprintf.c netcryptvoid.c
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/sbin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
