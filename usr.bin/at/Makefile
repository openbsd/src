#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:48:22 deraadt Exp $

PROG=	at
SRCS=	at.c panic.c parsetime.c perm.c
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
MLINKS=	at.1 batch.1 \
	at.1 atq.1 \
	at.1 atrm.1

BINOWN=	root
BINMODE= 4555

.include <bsd.prog.mk>
