# $OpenBSD: Makefile,v 1.7 2017/03/08 22:53:36 schwarze Exp $

REGRESS_TARGETS	= brokenbreaker twice tail two notopen
LINT_TARGETS	= brokenbreaker twice tail two notopen

# It's hard to keep stuff together in next-line scope.

SKIP_TMAN	= tail

# groff-1.22.3 defect:
# - non-matching enclosure end macro prints a closing delimiter

SKIP_GROFF	= brokenbreaker notopen

.include <bsd.regress.mk>
