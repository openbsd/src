#	$OpenBSD: Makefile,v 1.10 1997/07/25 21:56:16 millert Exp $
#	$NetBSD: Makefile,v 1.12 1997/07/20 09:45:35 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

# define SMALL to disable command line editing
#CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	stringlist.c util.c

LDADD+=	-ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}

#COPTS+= -Wall -Wconversion -Wstrict-prototypes -Wmissing-prototypes

.include <bsd.prog.mk>
