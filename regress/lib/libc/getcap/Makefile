#	$OpenBSD: Makefile,v 1.1 2006/03/10 05:28:57 ray Exp $

PROG=		getcaptest

REGRESS_TARGETS=shortcap

shortcap:
	./getcaptest ${.CURDIR}/shortcap.in

.include <bsd.regress.mk>
