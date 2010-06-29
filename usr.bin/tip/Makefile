#	$OpenBSD: Makefile,v 1.14 2010/06/29 20:09:39 nicm Exp $

PROG=	tip
LINKS=	${BINDIR}/tip ${BINDIR}/cu
MAN=	tip.1 cu.1

CFLAGS+=-I${.CURDIR} -DDEFBR=9600 -DDEFFS=BUFSIZ
LDADD=  -lutil
DPADD=  ${LIBUTIL}

SRCS=	cmds.c cmdtab.c cu.c hunt.c log.c partab.c remote.c tip.c \
	tipout.c value.c vars.c

.include <bsd.prog.mk>
