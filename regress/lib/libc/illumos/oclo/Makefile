#	$OpenBSD: Makefile,v 1.3 2026/06/24 12:24:04 bluhm Exp $

.if exists(/usr/local/share/illumos-os-tests)

MAKE :=		ulimit -n 256; ${MAKE}

PROGS =		oclo
PROGS +=	oclo_errors
PROGS +=	ocloexec_verify

LDADD_ocloexec_verify = -lkvm

WARNINGS =	yes

regress: ${PROGS}

.PATH: /usr/local/share/illumos-os-tests/tests/oclo
.endif

.include <bsd.regress.mk>
