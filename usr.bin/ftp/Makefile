#	$OpenBSD: Makefile,v 1.9 1997/04/10 00:17:06 millert Exp $
#	$NetBSD: Makefile,v 1.11 1997/03/24 21:59:36 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# define SMALL to disable command line editing
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	stringlist.c util.c

LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
