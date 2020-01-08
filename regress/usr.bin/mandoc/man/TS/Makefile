# $OpenBSD: Makefile,v 1.4 2020/01/08 10:17:15 schwarze Exp $

REGRESS_TARGETS	= break vspace
LINT_TARGETS	= break
GOPTS		= -t

# groff-1.22.3 defect:
# - Starting a table in next-line scope confuses font handling,
#   and in the case of .TP indentation as well.

SKIP_GROFF	= break

.include <bsd.regress.mk>
