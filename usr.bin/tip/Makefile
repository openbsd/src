#	$OpenBSD: Makefile,v 1.16 2013/03/10 17:55:54 nicm Exp $

PROG=	tip
MAN=	tip.1

CFLAGS+=-I${.CURDIR} -DDEFBR=9600 -DDEFFS=BUFSIZ
LDADD=  -lutil
DPADD=  ${LIBUTIL}

SRCS=	cmds.c cmdtab.c cu.c hunt.c log.c partab.c remote.c tip.c \
	tipout.c value.c

.include <bsd.prog.mk>
