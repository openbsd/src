# $OpenBSD: Makefile,v 1.2 2002/09/02 20:01:43 avsm Exp $

REGRESS_TARGETS=shcrash.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
