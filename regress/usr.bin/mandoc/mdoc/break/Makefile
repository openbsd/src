# $OpenBSD: Makefile,v 1.5 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	= brokenbreaker twice tail two notopen
LINT_TARGETS	= brokenbreaker twice tail two notopen
MARKDOWN_TARGETS = brokenbreaker twice two notopen

# It's hard to keep stuff together in next-line scope.

SKIP_TMAN	= tail

# groff-1.22.3 defect:
# - non-matching enclosure end macro prints a closing delimiter

SKIP_GROFF	= brokenbreaker notopen

.include <bsd.regress.mk>
