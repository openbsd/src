#	$OpenBSD: Makefile,v 1.1 2004/01/15 17:14:43 sturm Exp $

REGRESS_TARGETS=id.sh

id.sh:
	@sh ${.CURDIR}/id.sh ${.CURDIR}/id.policy

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
