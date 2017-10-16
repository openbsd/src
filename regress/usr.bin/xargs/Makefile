#	$OpenBSD: Makefile,v 1.4 2017/10/16 13:52:50 anton Exp $

REGRESS_TARGETS=run-regress-showargs run-regress-copyargs

XARGS=	/usr/bin/xargs

PROG= showargs

run-regress-showargs:
	sh ${.CURDIR}/xargs-L.sh ${XARGS}

run-regress-copyargs:
	sh ${.CURDIR}/xargs-copy.sh ${XARGS}

.include <bsd.regress.mk>
