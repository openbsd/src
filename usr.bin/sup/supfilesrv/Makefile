#	$Id: Makefile,v 1.1 1995/12/16 11:47:06 deraadt Exp $

.PATH:	${.CURDIR}/../src

PROG=	supfilesrv
MAN=	supservers.8
SRCS=	supfilesrv.o scan.o scm.o scmio.o stree.o log.o supmsg.o \
	atoo.o errmsg.o expand.o ffilecopy.o filecopy.o nxtarg.o \
	path.o quit.o run.o salloc.o skipto.o vprintf.o netcryptvoid.c
LDADD+=	-lutil -lcrypt
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/bin
CFLAGS+=-I${.CURDIR}/../src

.include <bsd.prog.mk>
