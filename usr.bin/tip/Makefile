#	$OpenBSD: Makefile,v 1.15 2010/07/02 07:40:03 nicm Exp $

PROG=	tip
LINKS=	${BINDIR}/tip ${BINDIR}/cu
MAN=	tip.1 cu.1

CFLAGS+=-I${.CURDIR} -DDEFBR=9600 -DDEFFS=BUFSIZ
LDADD=  -lutil
DPADD=  ${LIBUTIL}

SRCS=	cmds.c cmdtab.c cu.c hunt.c log.c partab.c remote.c tip.c \
	tipout.c value.c

.include <bsd.prog.mk>
