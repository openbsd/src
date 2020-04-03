#	$OpenBSD: Makefile,v 1.5 2020/04/03 12:01:56 bluhm Exp $

CLEANFILES += test.sig confirmorders confirmorders.sig HASH HASH.sig \
	      archive.tgz signed.tgz
REGRESS_TARGETS = t1

t1:
	sh ${.CURDIR}/signify.sh ${.CURDIR}

.PHONY: t1

.include <bsd.regress.mk>
