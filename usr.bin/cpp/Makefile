#	$OpenBSD: Makefile,v 1.13 2025/10/23 19:06:10 miod Exp $

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=

cpp: cpp.sh
	sed -e 's/@GNUC@//' ${.CURDIR}/cpp.sh >$@

.include <bsd.prog.mk>
