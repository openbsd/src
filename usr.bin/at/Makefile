#	$NetBSD: Makefile,v 1.2 1995/03/25 18:13:27 glass Exp $

PROG=	at
SRCS=	at.c panic.c parsetime.c
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
MLINKS=	at.1 batch.1 \
	at.1 atq.1 \
	at.1 atrm.1

BINOWN=	root
BINMODE= 4555

.include <bsd.prog.mk>
