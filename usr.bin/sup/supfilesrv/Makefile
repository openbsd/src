#	$OpenBSD: Makefile,v 1.5 1996/06/26 05:40:02 deraadt Exp $

PROG=	supfilesrv
MAN=	supservers.8
SRCS=	supfilesrv.c scan.c scm.c scmio.c stree.c log.c supmsg.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c salloc.c skipto.c vprintf.c netcryptvoid.c
#LDADD+=	-lutil -lcrypt
LDADD+=	-lutil
#DPADD+=	${LIBUTIL} ${LIBCRYPT}
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/sbin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
