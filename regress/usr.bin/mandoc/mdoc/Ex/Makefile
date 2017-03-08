# $OpenBSD: Makefile,v 1.3 2017/03/08 22:53:35 schwarze Exp $

REGRESS_TARGETS	 = args noname nostd
LINT_TARGETS	 = noname nostd

# groff-1.22.2 defect:
# - .Ex without -std produces no output

SKIP_GROFF	 = nostd

.include <bsd.regress.mk>
