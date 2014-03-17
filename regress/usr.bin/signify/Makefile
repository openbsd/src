#	$OpenBSD: Makefile,v 1.4 2014/03/17 02:49:02 tedu Exp $

CLEANFILES += test.sig confirmorders confirmorders.sig HASH.sig
REGRESS_TARGETS = t1

t1:
	@sh ${.CURDIR}/signify.sh ${.CURDIR}

.PHONY: t1

.include <bsd.regress.mk>
