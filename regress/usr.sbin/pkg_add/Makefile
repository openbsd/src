# $OpenBSD: Makefile,v 1.3 2006/08/18 13:14:34 espie Exp $

REGRESS_TARGETS=pkgnames depends-check

pkgnames:
	perl ${.CURDIR}/check-name

depends-check: old/a-0.0.tgz old/b-0.0.tgz new/a-1.0.tgz new/b-1.0.tgz
	-rm -rf ${.OBJDIR}/depends
	-mkdir -p ${.OBJDIR}/depends/pkgdb
	PKG_DBDIR=${.OBJDIR}/depends/pkgdb pkg_add -F nonroot -B${.OBJDIR}/depends old/a-0.0.tgz old/b-0.0.tgz
	PKG_DBDIR=${.OBJDIR}/depends/pkgdb PKG_PATH=${.OBJDIR}/new pkg_add -F nonroot -B${.OBJDIR}/depends -u a b
	PKG_DBDIR=${.OBJDIR}/depends/pkgdb PKG_PATH=${.OBJDIR}/old pkg_add -F nonroot -B${.OBJDIR}/depends -u a b

old/a-0.0.tgz:
	-mkdir -p ${@D}
	pkg_create -p /usr/local -A \* \
	    -DFULLPKGPATH=test/a \
	    -DPERMIT_PACKAGE_FTP=Yes \
	    -DPERMIT_PACKAGE_CDROM=Yes \
	    -P'test/b:b-*:b-0.0' \
	    -c '-comment' -d '-descr' -f ${.CURDIR}/empty $@

new/a-1.0.tgz:
	-mkdir -p ${@D}
	pkg_create -p /usr/local -A \* \
	    -DFULLPKGPATH=test/a \
	    -DPERMIT_PACKAGE_FTP=Yes \
	    -DPERMIT_PACKAGE_CDROM=Yes \
	    -c '-comment' -d '-descr' -f ${.CURDIR}/empty $@

old/b-0.0.tgz:
	-mkdir -p ${@D}
	pkg_create -p /usr/local -A \* \
	    -DFULLPKGPATH=test/a \
	    -DPERMIT_PACKAGE_FTP=Yes \
	    -DPERMIT_PACKAGE_CDROM=Yes \
	    -c '-comment' -d '-descr' -f ${.CURDIR}/empty $@

new/b-1.0.tgz:
	-mkdir -p ${@D}
	pkg_create -p /usr/local -A \* \
	    -DFULLPKGPATH=test/a \
	    -DPERMIT_PACKAGE_FTP=Yes \
	    -DPERMIT_PACKAGE_CDROM=Yes \
	    -P'test/a:a-*:a-1.0' \
	    -c '-comment' -d '-descr' -f ${.CURDIR}/empty $@

.PHONY: pkgnames depends-check

CLEANFILES+=old/*.tgz new/*.tgz depends/*/*/*

.include <bsd.regress.mk>
