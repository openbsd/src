# $OpenBSD: Makefile,v 1.9 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS  = badarg double eof literal longhead macrotag manyargs
REGRESS_TARGETS += sameline width

LINT_TARGETS	 = double eof

# groff-1.22.1 defect:
# - If the last line of the file is .TP, groff does not print a page footer.

SKIP_GROFF	 = eof

.include <bsd.regress.mk>
