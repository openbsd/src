# $OpenBSD: Makefile,v 1.2 2017/03/05 19:59:25 schwarze Exp $

REGRESS_TARGETS	 = args noname nostd
LINT_TARGETS	 = noname nostd
MARKDOWN_TARGETS = args noname nostd

# groff-1.22.2 defect:
# - .Ex without -std produces no output

SKIP_GROFF	 = nostd

.include <bsd.regress.mk>
