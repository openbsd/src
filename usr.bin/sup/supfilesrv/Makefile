#	$OpenBSD: Makefile,v 1.10 2001/05/05 15:32:11 millert Exp $

PROG=	supfilesrv
MAN=	supservers.8
MLINKS=	supservers.8 supfilesrv.8
MLINKS+=supservers.8 supscan.8
SRCS=	supfilesrv.c scan.c scm.c scmio.c stree.c log.c supmsg.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c skipto.c vprintf.c netcryptvoid.c
LDADD+=	-lwrap
DPADD+=	${LIBWRAP}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/sbin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
