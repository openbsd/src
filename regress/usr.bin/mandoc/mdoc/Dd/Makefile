# $OpenBSD: Makefile,v 1.2 2014/11/21 01:52:45 schwarze Exp $

REGRESS_TARGETS	 = badarg dupe late long manarg noarg order
LINT_TARGETS	 = badarg dupe late long manarg noarg order

# If groff finds exactly three arguments, it assumes they are month,
# day and year without further checking.  If there are no arguments,
# groff uses the string "Epoch".  Otherwise, it silently falls back
# to today's date.
# That is not at all sane behaviour, we are not going to imitate it.

SKIP_GROFF	 = badarg long manarg noarg
SKIP_ASCII	 = noarg

# Autodetection fails for late .Dd, so specify -mdoc explicitly.

MOPTS		+= -mdoc

.include <bsd.regress.mk>
