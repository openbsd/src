#	$OpenBSD: Makefile,v 1.7 1997/03/26 00:35:38 deraadt Exp $

PROG=	supfilesrv
MAN=	supservers.8
MLINKS=	supservers.8 supfilesrv.8
MLINKS+=supservers.8 supscan.8
SRCS=	supfilesrv.c scan.c scm.c scmio.c stree.c log.c supmsg.c \
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
