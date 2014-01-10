#	$OpenBSD: Makefile,v 1.2 2014/01/10 22:52:50 tobiasu Exp $

CLEANFILES += test.sig
REGRESS_TARGETS = t1

t1:
	@sh ${.CURDIR}/signify.sh ${.CURDIR}

.PHONY: t1

.include <bsd.regress.mk>
