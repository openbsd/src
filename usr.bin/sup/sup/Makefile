#	$Id: Makefile,v 1.1 1995/12/16 11:47:05 deraadt Exp $

.PATH:	${.CURDIR}/../src

PROG=	sup
MAN=	sup.1
SRCS=	supcmain.o supcvers.o supcparse.o supcname.o supcmisc.o supcmeat.o \
	scm.o scmio.o stree.o log.o supmsg.o \
	atoo.o errmsg.o expand.o ffilecopy.o filecopy.o nxtarg.o \
	path.o quit.o run.o salloc.o skipto.o vprintf.o netcryptvoid.c
LDADDR+=-lutil
DPADD+=	${LIBUTIL}
BINOWN=	root
BINGRP=	daemon
BINMODE=555
BINDIR=	/usr/bin
CFLAGS+=-I${.CURDIR}/../src

.include <bsd.prog.mk>
