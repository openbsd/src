#	$OpenBSD: Makefile,v 1.3 2017/10/16 13:48:22 anton Exp $

XARGS=	/usr/bin/xargs

PROG= showargs

run-regress-showargs:
	sh ${.CURDIR}/xargs-L.sh ${XARGS}

.include <bsd.regress.mk>
