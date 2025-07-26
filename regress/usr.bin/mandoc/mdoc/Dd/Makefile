# $OpenBSD: Makefile,v 1.7 2025/07/26 08:55:09 schwarze Exp $

REGRESS_TARGETS	 = badarg dupe late long manarg noarg order
LINT_TARGETS	 = badarg dupe late long manarg noarg order

# Autodetection fails for late .Dd, so specify -mdoc explicitly.

MOPTS		 = -mdoc

# groff-1.23.0 defect:
#  - footer fields of excessive length get printed on top of each other
#    rather than breaking the output line.

SKIP_GROFF	 = long

# groff-1.23.0 regression:
#  - .Dd always ends the manual page and starts a new one

SKIP_GROFF	+= dupe late

.include <bsd.regress.mk>
