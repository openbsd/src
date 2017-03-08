# $OpenBSD: Makefile,v 1.8 2017/03/08 22:53:35 schwarze Exp $

REGRESS_TARGETS	 = badarg break broken inputlines lines synopsis
LINT_TARGETS	 = badarg broken

SKIP_TMAN ?= badarg

# mandoc -T markdown output of .Bk is quite ugly,
# so refrain from testing it for now

SKIP_MARKDOWN ?= ALL

.include <bsd.regress.mk>
