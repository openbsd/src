#	$OpenBSD: Makefile,v 1.4 2001/05/02 22:56:54 millert Exp $

PROG=	sup
MAN=	sup.1
SRCS=	supcmain.c supcvers.c supcparse.c supcname.c supcmisc.c supcmeat.c \
	scm.c scmio.c stree.c log.c supmsg.c read_line.c \
	atoo.c errmsg.c expand.c ffilecopy.c filecopy.c nxtarg.c \
	path.c quit.c run.c salloc.c skipto.c vprintf.c netcryptvoid.c
LDADD+=-lutil
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/bin
CFLAGS+=-I${.CURDIR}/../src
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
