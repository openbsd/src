#	$Id: Makefile,v 1.2 1995/12/17 12:58:14 deraadt Exp $

PROG=	supfilesrv
MAN=	supservers.8
SRCS=	supfilesrv.c scan.c scm.c scmio.c stree.c log.c supmsg.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c salloc.c skipto.c vprintf.c netcryptvoid.c
LDADD+=	-lutil -lcrypt
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/bin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
