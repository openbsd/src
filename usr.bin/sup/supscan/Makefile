#	$Id: Makefile,v 1.3 1996/02/03 10:05:06 deraadt Exp $

PROG=	supscan
NOMAN=
SRCS=	supscan.c stree.c scan.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c salloc.c skipto.c vprintf.c netcryptvoid.c
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/sbin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
