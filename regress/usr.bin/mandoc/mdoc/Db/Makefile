# $OpenBSD: Makefile,v 1.2 2017/03/08 22:53:35 schwarze Exp $

REGRESS_TARGETS	= args
LINT_TARGETS	= args

# mandoc -T markdown ignores .Db

SKIP_MARKDOWN ?= ALL

.include <bsd.regress.mk>
