# $OpenBSD: Makefile,v 1.6 2020/01/19 16:16:33 schwarze Exp $

REGRESS_TARGETS	 = badarg dupe late long manarg noarg order
LINT_TARGETS	 = badarg dupe late long manarg noarg order

# groff-1.22.4 prints footer fields of excessive length on top of
# each other rather than breaking the output line.

SKIP_GROFF	 = long

# Autodetection fails for late .Dd, so specify -mdoc explicitly.

MOPTS		 = -mdoc

.include <bsd.regress.mk>
