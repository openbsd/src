# $OpenBSD: Makefile,v 1.5 2025/07/24 17:16:34 schwarze Exp $

REGRESS_TARGETS	= break vspace
LINT_TARGETS	= break
GOPTS		= -t

# groff-1.23.0 defect:
# - Starting a table in next-line scope confuses font handling,
#   and in the case of .TP indentation as well.

SKIP_GROFF	= break

.include <bsd.regress.mk>
