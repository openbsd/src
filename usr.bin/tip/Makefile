#	$OpenBSD: Makefile,v 1.17 2015/02/07 10:07:15 deraadt Exp $

PROG=	tip
MAN=	tip.1

CFLAGS+=-I${.CURDIR} -DDEFBR=9600 -DDEFFS=BUFSIZ
LDADD=  -lutil
DPADD=  ${LIBUTIL}

SRCS=	cmds.c cmdtab.c hunt.c log.c partab.c remote.c tip.c \
	tipout.c value.c

.include <bsd.prog.mk>
