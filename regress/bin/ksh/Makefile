# $OpenBSD: Makefile,v 1.5 2010/03/24 08:29:44 fgsch Exp $

REGRESS_TARGETS=shcrash.sh seterror.sh varfunction.sh eval.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

seterror.sh:
	sh ${.CURDIR}/seterror.sh

varfunction.sh:
	sh ${.CURDIR}/varfunction.sh

eval.sh:
	test `sh ${.CURDIR}/eval.sh | wc -l` == 4

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
