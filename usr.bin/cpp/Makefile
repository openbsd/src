#	$OpenBSD: Makefile,v 1.11 2013/08/06 19:11:53 miod Exp $

.include <bsd.own.mk>

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=

cpp: cpp.sh
.if ${COMPILER_VERSION:L} == "gcc3"
	sed -e 's/@GNUC@/-D__GNUC__/' -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@
.else
	sed -e 's/@GNUC@//' -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@
.endif

.include <bsd.prog.mk>
