# $OpenBSD: Makefile,v 1.5 2020/01/08 10:17:15 schwarze Exp $

REGRESS_TARGETS	 = badarg dupe late long manarg noarg order
LINT_TARGETS	 = badarg dupe late long manarg noarg order

# noarg output contains the date when the file is formatted

SKIP_ASCII	?= noarg
SKIP_MARKDOWN	?= noarg

# If groff finds exactly three arguments, it assumes they are month,
# day and year without further checking.  If there are no arguments,
# groff uses the string "Epoch".  Otherwise, it silently falls back
# to today's date.
# That is not at all sane behaviour, we are not going to imitate it.

SKIP_GROFF	 = badarg long manarg noarg

# Autodetection fails for late .Dd, so specify -mdoc explicitly.

MOPTS		 = -mdoc

.include <bsd.regress.mk>
