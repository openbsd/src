#	$OpenBSD: Makefile,v 1.3 2014/01/13 22:29:32 tedu Exp $

CLEANFILES += test.sig confirmorders confirmorders.sig
REGRESS_TARGETS = t1

t1:
	@sh ${.CURDIR}/signify.sh ${.CURDIR}

.PHONY: t1

.include <bsd.regress.mk>
