#	$OpenBSD: Makefile,v 1.5 2001/05/04 22:16:17 millert Exp $

PROG=	sup
MAN=	sup.1
SRCS=	supcmain.c supcvers.c supcparse.c supcname.c supcmisc.c supcmeat.c \
	scm.c scmio.c stree.c log.c supmsg.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c skipto.c vprintf.c netcryptvoid.c
LDADD+=-lutil
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/bin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
