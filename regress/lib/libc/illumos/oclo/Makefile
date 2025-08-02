#	$OpenBSD: Makefile,v 1.1.1.1 2025/08/02 06:16:34 tb Exp $

.if exists(/usr/local/share/illumos-os-tests)

PROGS =		oclo
PROGS +=	oclo_errors
PROGS +=	ocloexec_verify

LDADD_ocloexec_verify = -lkvm

WARNINGS =	yes

.PATH: /usr/local/share/illumos-os-tests/tests/oclo
.endif

.include <bsd.regress.mk>
