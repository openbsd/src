# $OpenBSD: Makefile,v 1.4 2009/11/14 09:34:56 espie Exp $

REGRESS_TARGETS=pkgnames depends-check

PKG_ADD=perl ${.CURDIR}/myadd
PKG_CREATE=perl ${.CURDIR}/mycreate

pkgnames:
	perl ${.CURDIR}/check-name

depends-check: old/a-0.0.tgz old/b-0.0.tgz new/a-1.0.tgz new/b-1.0.tgz
	-rm -rf ${.OBJDIR}/depends
	ROOT=${.OBJDIR}/depends ${PKG_ADD} old/a-0.0.tgz old/b-0.0.tgz
	ROOT=${.OBJDIR}/depends PKG_PATH=${.OBJDIR}/new ${PKG_ADD} -u a b
	ROOT=${.OBJDIR}/depends PKG_PATH=${.OBJDIR}/old ${PKG_ADD} -F downgrade -u a b

old/a-0.0.tgz:
	${PKG_CREATE} -P'test/b:b-*:b-0.0' -f ${.CURDIR}/empty $@

new/a-1.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

old/b-0.0.tgz:
	${PKG_CREATE} -f ${.CURDIR}/empty $@

new/b-1.0.tgz:
	${PKG_CREATE} -P'test/a:a-*:a-1.0' -f ${.CURDIR}/empty $@

.PHONY: pkgnames depends-check

CLEANFILES+=old/*.tgz new/*.tgz depends/*/*/*

.include <bsd.regress.mk>
