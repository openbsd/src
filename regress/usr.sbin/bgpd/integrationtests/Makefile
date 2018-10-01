# $OpenBSD: Makefile,v 1.1 2018/10/01 20:58:59 benno Exp $

REGRESS_TARGETS = network_statement

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh

.include <bsd.regress.mk>
