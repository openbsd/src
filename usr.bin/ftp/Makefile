#	$OpenBSD: Makefile,v 1.8 1997/03/26 00:35:23 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1997/03/14 01:39:31 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# define SMALL to disable command line editing
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	stringlist.c util.c

LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
