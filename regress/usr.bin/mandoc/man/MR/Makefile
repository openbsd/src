# $OpenBSD: Makefile,v 1.1 2023/10/24 20:30:49 schwarze Exp $

REGRESS_TARGETS	 = basic
LINT_TARGETS	 = basic

.include <bsd.regress.mk>

# temporary hack while working towards an update of the groff port
MANDOC = mandoc -O indent=5
GROFF = /co/groff/build/test-groff ${GOPTS} -d MF=R ${MOPTS} -Wall -P-c
