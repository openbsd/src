# $OpenBSD: Makefile,v 1.1 2002/02/27 19:57:17 dhartmei Exp $

REGRESSTARGETS=shcrash.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
