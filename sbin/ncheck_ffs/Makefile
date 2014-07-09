#	$OpenBSD: Makefile,v 1.4 2014/07/09 11:30:13 krw Exp $

PROG=	ncheck_ffs
DPADD+=	${LIBUTIL}
LDADD+=	-lutil
MAN=	ncheck_ffs.8

LINKS=	${BINDIR}/ncheck_ffs ${BINDIR}/ncheck
MLINKS=	ncheck_ffs.8 ncheck.8

.include <bsd.prog.mk>
