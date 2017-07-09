#	$OpenBSD: Makefile,v 1.3 2017/07/09 21:23:19 espie Exp $
#	$NetBSD: Makefile,v 1.1 2002/03/01 11:21:58 ad Exp $

PROG=	pkill

LDADD+=	-lkvm
DPADD+=	${LIBKVM}

LINKS+=	${BINDIR}/pkill ${BINDIR}/pgrep

.include <bsd.prog.mk>
