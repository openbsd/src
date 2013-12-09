#	$OpenBSD: Makefile,v 1.12 2013/12/09 02:35:09 guenther Exp $

.include <bsd.own.mk>

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=

cpp: cpp.sh
.if ${COMPILER_VERSION:L} == "gcc3"
	sed -e 's/@GNUC@/-D__GNUC__/' ${.CURDIR}/cpp.sh >$@
.else
	sed -e 's/@GNUC@//' ${.CURDIR}/cpp.sh >$@
.endif

.include <bsd.prog.mk>
